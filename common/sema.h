//---------------------------------------------------------
// For conditions of distribution and use, see
// https://github.com/preshing/cpp11-on-multicore/blob/master/LICENSE
//---------------------------------------------------------

#ifndef __CPP11OM_SEMAPHORE_H__
#define __CPP11OM_SEMAPHORE_H__

#include <atomic>
#include <cassert>
#include <type_traits>


#if defined(_WIN32)
//---------------------------------------------------------
// Semaphore (Windows)
//---------------------------------------------------------

// Avoid including windows.h in a header; we only need a handful of
// items, so we'll redeclare them here (this is relatively safe since
// the API generally has to remain stable between Windows versions).
// I know this is an ugly hack but it still beats polluting the global
// namespace with thousands of generic names or adding a .cpp for nothing.
extern "C" {
    struct _SECURITY_ATTRIBUTES;
    __declspec(dllimport) void* __stdcall CreateSemaphoreW(_SECURITY_ATTRIBUTES* lpSemaphoreAttributes, long lInitialCount, long lMaximumCount, const wchar_t* lpName);
    __declspec(dllimport) int __stdcall CloseHandle(void* hObject);
    __declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void* hHandle, unsigned long dwMilliseconds);
    __declspec(dllimport) int __stdcall ReleaseSemaphore(void* hSemaphore, long lReleaseCount, long* lpPreviousCount);
}

class Semaphore
{
private:
    void* m_hSema;

    Semaphore(const Semaphore& other) = delete;
    Semaphore& operator=(const Semaphore& other) = delete;

public:
    Semaphore(int initialCount = 0)
    {
        assert(initialCount >= 0);
        const long maxLong = 0x7fffffff;
        m_hSema = CreateSemaphoreW(nullptr, initialCount, maxLong, nullptr);
    }

    ~Semaphore()
    {
        CloseHandle(m_hSema);
    }

    void wait()
    {
        const unsigned long infinite = 0xffffffff;
        WaitForSingleObject(m_hSema, infinite);
    }

    void signal(int count = 1)
    {
        ReleaseSemaphore(m_hSema, count, nullptr);
    }
};


#elif defined(__MACH__)
//---------------------------------------------------------
// Semaphore (Apple iOS and OSX)
// Can't use POSIX semaphores due to http://lists.apple.com/archives/darwin-kernel/2009/Apr/msg00010.html
//---------------------------------------------------------

#include <mach/mach.h>

class Semaphore
{
private:
    semaphore_t m_sema;

    Semaphore(const Semaphore& other) = delete;
    Semaphore& operator=(const Semaphore& other) = delete;

public:
    Semaphore(int initialCount = 0)
    {
        assert(initialCount >= 0);
        semaphore_create(mach_task_self(), &m_sema, SYNC_POLICY_FIFO, initialCount);
    }

    ~Semaphore()
    {
        semaphore_destroy(mach_task_self(), m_sema);
    }

    void wait()
    {
        semaphore_wait(m_sema);
    }

    void signal()
    {
        semaphore_signal(m_sema);
    }

    void signal(int count)
    {
        while (count-- > 0)
        {
            semaphore_signal(m_sema);
        }
    }
};


#elif defined(__unix__)
//---------------------------------------------------------
// Semaphore (POSIX, Linux)
//---------------------------------------------------------

#include <semaphore.h>

class Semaphore
{
private:
    sem_t m_sema;

    Semaphore(const Semaphore& other) = delete;
    Semaphore& operator=(const Semaphore& other) = delete;

public:
    Semaphore(int initialCount = 0)
    {
        assert(initialCount >= 0);
        sem_init(&m_sema, 0, initialCount);
    }

    ~Semaphore()
    {
        sem_destroy(&m_sema);
    }

    void wait()
    {
        // http://stackoverflow.com/questions/2013181/gdb-causes-sem-wait-to-fail-with-eintr-error
        int rc;
        do
        {
            rc = sem_wait(&m_sema);
        }
        while (rc == -1 && errno == EINTR);
    }

    void signal()
    {
        sem_post(&m_sema);
    }

    void signal(int count)
    {
        while (count-- > 0)
        {
            sem_post(&m_sema);
        }
    }
};


#else

#error Unsupported platform!

#endif


//---------------------------------------------------------
// LightweightSemaphore
//---------------------------------------------------------
class LightweightSemaphore
{
public:
    // The underlying semaphores are limited to int-sized counts,
    // but there's no reason we can't scale higher on platforms with
    // a wider size_t than int -- the only counts we pass on to the
    // underlying semaphores are the number of waiting threads, which
    // will always fit in an int for all platforms regardless of our
    // high-level count.
    typedef std::make_signed<std::size_t>::type ssize_t;
    
private:
    std::atomic<ssize_t> m_count;
    Semaphore m_sema;

    void waitWithPartialSpinning()
    {
        ssize_t oldCount;
        // Is there a better way to set the initial spin count?
        // If we lower it to 1000, testBenaphore becomes 15x slower on my Core i7-5930K Windows PC,
        // as threads start hitting the kernel semaphore.
        int spin = 10000;
        while (spin--)
        {
            oldCount = m_count.load(std::memory_order_relaxed);
            if ((oldCount > 0) && m_count.compare_exchange_strong(oldCount, oldCount - 1, std::memory_order_acquire, std::memory_order_relaxed))
                return;
            std::atomic_signal_fence(std::memory_order_acquire);     // Prevent the compiler from collapsing the loop.
        }
        oldCount = m_count.fetch_sub(1, std::memory_order_acquire);
        if (oldCount <= 0)
        {
            m_sema.wait();
        }
    }

    ssize_t waitManyWithPartialSpinning(ssize_t max)
    {
        assert(max > 0);
        ssize_t oldCount;
        int spin = 10000;
        while (spin--)
        {
            oldCount = m_count.load(std::memory_order_relaxed);
            if (oldCount > 0)
            {
                ssize_t newCount = oldCount > max ? oldCount - max : 0;
                if (m_count.compare_exchange_strong(oldCount, newCount, std::memory_order_acquire, std::memory_order_relaxed))
                    return oldCount - newCount;
            }
            std::atomic_signal_fence(std::memory_order_acquire);
        }
        oldCount = m_count.fetch_sub(1, std::memory_order_acquire);
        if (oldCount <= 0)
            m_sema.wait();
        if (max > 1)
            return 1 + tryWaitMany(max - 1);
        return 1;
    }

public:
    LightweightSemaphore(ssize_t initialCount = 0) : m_count(initialCount)
    {
        assert(initialCount >= 0);
    }

    bool tryWait()
    {
        ssize_t oldCount = m_count.load(std::memory_order_relaxed);
        while (oldCount > 0)
        {
            if (m_count.compare_exchange_weak(oldCount, oldCount - 1, std::memory_order_acquire, std::memory_order_relaxed))
                return true;
        }
        return false;
    }

    void wait()
    {
        if (!tryWait())
            waitWithPartialSpinning();
    }
    
    // Acquires between 0 and (greedily) max, inclusive
    ssize_t tryWaitMany(ssize_t max)
    {
        assert(max >= 0);
        ssize_t oldCount = m_count.load(std::memory_order_relaxed);
        while (oldCount > 0)
        {
            ssize_t newCount = oldCount > max ? oldCount - max : 0;
            if (m_count.compare_exchange_weak(oldCount, newCount, std::memory_order_acquire, std::memory_order_relaxed))
                return oldCount - newCount;
        }
        return 0;
    }
    
    // Acquires at least one, and (greedily) at most max
    ssize_t waitMany(ssize_t max)
    {
        assert(max >= 0);
        ssize_t result = tryWaitMany(max);
        if (result == 0 && max > 0)
            result = waitManyWithPartialSpinning(max);
        return result;
    }

    void signal(ssize_t count = 1)
    {
        assert(count >= 0);
        ssize_t oldCount = m_count.fetch_add(count, std::memory_order_release);
        ssize_t toRelease = -oldCount < count ? -oldCount : count;
        if (toRelease > 0)
        {
            m_sema.signal((int)toRelease);
        }
    }
    
    ssize_t availableApprox() const
    {
        ssize_t count = m_count.load(std::memory_order_relaxed);
        return count > 0 ? count : 0;
    }
};


typedef LightweightSemaphore DefaultSemaphoreType;


#endif // __CPP11OM_SEMAPHORE_H__
