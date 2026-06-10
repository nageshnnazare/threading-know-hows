// 01_race_condition.cpp -- The original sin: shared mutable state.
//
//  ++counter is NOT atomic. It's at least three machine ops:
//
//        load  reg, [counter]      <-- read
//        add   reg, 1
//        store [counter], reg      <-- write
//
//  Two threads can interleave:
//
//      thread A           thread B
//      load 5             load 5
//      add  6             add  6
//      store 6            store 6     -> counter ends up 6 (lost update)
//
//  Fix: protect the increment so the load-add-store is indivisible.
//      - std::mutex                (correct, slowest under contention)
//      - std::atomic<int>          (correct, fast)
//      - hardware-locked RMW       (`__atomic_fetch_add`, std::atomic_ref)
//
//  This file shows the bug, then the two cures, side by side.

#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

constexpr int N = 8, ITERS = 200000;

template <class F>
long bench(F f)
{
    std::vector<std::thread> ts;
    for (int i = 0; i < N; ++i) ts.emplace_back(f);
    for (auto& t : ts) t.join();
    return 0;
}

int main()
{
    /* (1) BUG: unsynchronized */
    {
        long c = 0;
        bench([&]{ for (int i = 0; i < ITERS; ++i) ++c; });
        std::cout << "(1) unsynchronized = " << c
                  << " (expected " << long(N)*ITERS << " -- usually wrong!)\n";
    }

    /* (2) FIX with std::mutex */
    {
        long c = 0; std::mutex m;
        bench([&]{ for (int i = 0; i < ITERS; ++i) { std::lock_guard g(m); ++c; }});
        std::cout << "(2) std::mutex     = " << c << "\n";
    }

    /* (3) FIX with std::atomic */
    {
        std::atomic<long> c{0};
        bench([&]{ for (int i = 0; i < ITERS; ++i) ++c; });
        std::cout << "(3) std::atomic    = " << c.load() << "\n";
    }
}
