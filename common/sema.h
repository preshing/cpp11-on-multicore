//---------------------------------------------------------
// For conditions of distribution and use, see
// https://github.com/preshing/cpp11-on-multicore/blob/master/LICENSE
//---------------------------------------------------------

#ifndef __CPP11OM_SEMAPHORE_H__
#define __CPP11OM_SEMAPHORE_H__

#include <atomic>
#include <cassert>
#include <cstdint>


#if defined(_WIN32)
//---------------------------------------------------------
// Semaphore (Windows)
//---------------------------------------------------------

#define NOMINMAX
#include <windows.h>

class Semaphore
{
private:
    HANDLE m_hSema;

    Semaphore(const Semaphore& other) = delete;
    Semaphore& operator=(const Semaphore& other) = delete;

public:
    Semaphore(int initialCount = 0)
    {
        assert(initialCount >= 0);
        m_hSema = CreateSemaphore(NULL, initialCount, MAXLONG, NULL);
    }

    ~Semaphore()
    {
        CloseHandle(m_hSema);
    }

    void wait()
    {
        WaitForSingleObject(m_hSema, INFINITE);
    }

    bool try_wait()
    {
        return WaitForSingleObject(m_hSema, 0) != RC_WAIT_TIMEOUT;
    }

    bool timed_wait(std::uint64_t usecs)
    {
        return WaitForSingleObject(m_hSema, (unsigned long)(usecs / 1000)) != RC_WAIT_TIMEOUT;
    }

    void signal(int count = 1)
    {
        ReleaseSemaphore(m_hSema, count, NULL);
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

    bool try_wait()
    {
        return timed_wait(0);
    }
    
    bool timed_wait(std::uint64_t timeout_usecs)
    {
        mach_timespec_t ts;
        ts.tv_sec = timeout_usecs / 1000000;
        ts.tv_nsec = (timeout_usecs % 1000000) * 1000;

        // added in OSX 10.10: https://developer.apple.com/library/prerelease/mac/documentation/General/Reference/APIDiffsMacOSX10_10SeedDiff/modules/Darwin.html
        kern_return_t rc = semaphore_timedwait(m_sema, ts);

        return rc != KERN_OPERATION_TIMED_OUT;
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
#include <cerrno>

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

    bool try_wait()
    {
        int rc;
        do
        {
            rc = sem_trywait(&m_sema);
        }
        while (rc == -1 && errno == EINTR);
        return !(rc == -1 && errno == EAGAIN);
    }

    bool timed_wait(std::uint64_t usecs)
    {
        struct timespec ts;
        const int usecs_in_1_sec = 1000000;
        const int nsecs_in_1_sec = 1000000000;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += usecs / usecs_in_1_sec;
        ts.tv_nsec += (usecs % usecs_in_1_sec) * 1000;
        // sem_timedwait bombs if you have more than 1e9 in tv_nsec
        // so we have to clean things up before passing it in
        if (ts.tv_nsec > nsecs_in_1_sec)
        {
            ts.tv_nsec -= nsecs_in_1_sec;
            ++ts.tv_sec;
        }

        int rc;
        do
        {
            rc = sem_timedwait(&m_sema, &ts);
        }
        while (rc == -1 && errno == EINTR);
        return !(rc == -1 && errno == ETIMEDOUT);
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
private:
    std::atomic<int> m_count;
    Semaphore m_sema;

    bool waitWithPartialSpinning(std::int64_t timeout_usecs = -1)
    {
        int oldCount;
        // Is there a better way to set the initial spin count?
        // If we lower it to 1000, testBenaphore becomes 15x slower on my Core i7-5930K Windows PC,
        // as threads start hitting the kernel semaphore.
        int spin = 10000;
        while (spin--)
        {
            oldCount = m_count.load(std::memory_order_relaxed);
            if ((oldCount > 0) && m_count.compare_exchange_strong(oldCount, oldCount - 1, std::memory_order_acquire))
                return true;
            std::atomic_signal_fence(std::memory_order_acquire);     // Prevent the compiler from collapsing the loop.
        }
        oldCount = m_count.fetch_sub(1, std::memory_order_acquire);
        if (oldCount > 0)
            return true;
        if (timeout_usecs < 0)
        {
            m_sema.wait();
            return true;
        }
        if (m_sema.timed_wait((std::uint64_t)timeout_usecs))
            return true;
        // At this point, we've timed out waiting for the semaphore, but the
        // count is still decremented indicating we may still be waiting on
        // it. So we have to re-adjust the count, but only if the semaphore
        // wasn't signaled enough times for us too since then. If it was, we
        // need to release the semaphore too.
        while (true)
        {
            oldCount = m_count.load(std::memory_order_acquire);
            if (oldCount >= 0 && m_sema.try_wait())
                return true;
            if (oldCount < 0 && m_count.compare_exchange_strong(oldCount, oldCount + 1, std::memory_order_relaxed))
                return false;
        }
    }

public:
    LightweightSemaphore(int initialCount = 0) : m_count(initialCount)
    {
        assert(initialCount >= 0);
    }

    bool tryWait()
    {
        int oldCount = m_count.load(std::memory_order_relaxed);
        return (oldCount > 0 && m_count.compare_exchange_strong(oldCount, oldCount - 1, std::memory_order_acquire));
    }

    void wait()
    {
        if (!tryWait())
            waitWithPartialSpinning();
    }

    bool wait(std::int64_t timeout_usecs)
    {
        return tryWait() || waitWithPartialSpinning(timeout_usecs);
    }

    void signal(int count = 1)
    {
        int oldCount = m_count.fetch_add(count, std::memory_order_release);
        int toRelease = -oldCount < count ? -oldCount : count;
        if (toRelease > 0)
        {
            m_sema.signal(toRelease);
        }
    }
};


typedef LightweightSemaphore DefaultSemaphoreType;


#endif // __CPP11OM_SEMAPHORE_H__
