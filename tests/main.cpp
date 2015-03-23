//---------------------------------------------------------
// For conditions of distribution and use, see
// https://github.com/preshing/cpp11-on-multicore/blob/master/LICENSE
//---------------------------------------------------------

#include <chrono>
#include <iostream>


//---------------------------------------------------------
// List of tests
//---------------------------------------------------------
struct TestInfo
{
    const char* name;
    bool (*testFunc)();
};

bool testBenaphore();
bool testRecursiveBenaphore();
bool testAutoResetEvent();
bool testRWLock();
bool testRWLockSimple();
bool testDiningPhilosophers();

#define ADD_TEST(name) { #name, name },
TestInfo g_tests[] =
{
    ADD_TEST(testBenaphore)
    ADD_TEST(testRecursiveBenaphore)
    ADD_TEST(testAutoResetEvent)
    ADD_TEST(testRWLock)
    ADD_TEST(testRWLockSimple)
    ADD_TEST(testDiningPhilosophers)
};

#include "bitfield.h"
    BEGIN_BITFIELD_TYPE(Status, uint32_t)
        ADD_BITFIELD_ARRAY(philos, 0, 4, 8)
    END_BITFIELD_TYPE()

    __declspec(noinline) void foo(Status& s, int index)
    {
        s.philos[index] = 7;
    }

//---------------------------------------------------------
// main
//---------------------------------------------------------
int main()
{
    bool allTestsPassed = true;

    Status s;
    foo(s, 5);
    std::cout << Status().philos.maximum() << std::endl;

    for (const TestInfo& test : g_tests)
    {
        std::cout << "Running " << test.name << "...";

        auto start = std::chrono::high_resolution_clock::now();
        bool result = test.testFunc();
        auto end = std::chrono::high_resolution_clock::now();

        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << " " << (result ? "passed" : "***FAILED***") << " in " << millis << " ms\n";
        allTestsPassed = allTestsPassed && result;
    }

    return allTestsPassed ? 0 : 1;
}
