// 01_thread_local.cpp -- thread_local storage class.
//
//  A thread_local variable has a SEPARATE INSTANCE in every thread, with
//  its own lifetime tied to that thread. Same syntax as static, but
//  per-thread instead of per-process.
//
//      thread_local int counter = 0;     // each thread starts with 0
//
//  Memory diagram:
//
//      shared (static):    +-----+
//                          |  X  |   <-- one instance, all threads share
//                          +-----+
//
//      thread_local:       +-----+    +-----+    +-----+
//                          | X1  |    | X2  |    | X3  |
//                          +-----+    +-----+    +-----+
//                            T1         T2         T3
//
//  Use cases:
//      - Per-thread RNG.
//      - Per-thread allocator/arena.
//      - Per-thread caches.
//      - Per-thread logging buffer.
//
//  Caveats:
//      - First access in each thread can be slower (TLS slot lookup,
//        constructor on first use).
//      - For non-trivial types, the destructor runs at thread exit.

#include <iostream>
#include <thread>
#include <vector>
#include <random>

thread_local std::mt19937 rng{std::random_device{}()};
thread_local int          calls = 0;

int rand_in(int lo, int hi)
{
    ++calls;
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng);
}

int main()
{
    std::vector<std::thread> ts;
    for (int i = 0; i < 4; ++i) {
        ts.emplace_back([i]{
            int sum = 0;
            for (int k = 0; k < 5; ++k) sum += rand_in(1, 100);
            std::cout << "thread " << i
                      << " sum=" << sum
                      << " calls=" << calls << "\n";
        });
    }
    for (auto& t : ts) t.join();
    // main thread's `calls` is still 0 (its own per-thread instance).
    std::cout << "main calls=" << calls << "\n";
}
