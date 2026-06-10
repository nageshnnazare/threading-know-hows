// 03_readers_writers.cpp -- Demonstration of reader/writer access patterns.
//
//  We compare three approaches for a read-heavy data structure (a counter
//  that's read MUCH more often than written):
//
//   (A) plain std::mutex    -- correct but readers serialize
//   (B) std::shared_mutex   -- many readers in parallel; writer exclusive
//   (C) std::atomic<int>    -- no lock at all (when fits a word)
//
//  Run and compare timings (rough). Lessons:
//
//   * If the value fits in a word: atomics win.
//   * If it's a small struct read often: shared_mutex.
//   * Don't reach for shared_mutex without measuring -- on low core counts
//     a normal mutex can be faster due to its lower overhead.
//
//  Picture:
//
//      time --->
//      mutex:        R==R==R==R==W==R==R==R   (always one at a time)
//      shared_mutex: R         R         W   R         R
//                    R     R   R             R   R     R
//                    R   R     R             R   R     R   (parallel reads)
//      atomic:       R R R R R R R W R R R R    (zero serialization)

#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <vector>
#include <numeric>

constexpr int READERS = 8;
constexpr int WRITERS = 1;
constexpr int READS_PER_R = 200000;
constexpr int WRITES_PER_W = 1000;

template <class Lock, class Value, class ReadFn, class WriteFn>
double bench(const char* name, Value& v, Lock& lk, ReadFn rd, WriteFn wr)
{
    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    long long total = 0;
    std::mutex sum_m;

    for (int i = 0; i < READERS; ++i)
        ts.emplace_back([&]{
            long long my = 0;
            for (int k = 0; k < READS_PER_R; ++k) my += rd(v, lk);
            std::lock_guard g(sum_m);
            total += my;
        });
    for (int i = 0; i < WRITERS; ++i)
        ts.emplace_back([&]{
            for (int k = 0; k < WRITES_PER_W; ++k) wr(v, lk, k);
        });
    for (auto& t : ts) t.join();
    auto dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::cout << name << ": " << dt << " sec  total=" << total << "\n";
    return dt;
}

int main()
{
    /* (A) plain mutex */
    {
        std::mutex m;
        int v = 0;
        bench("(A) mutex      ", v, m,
            [](int& v, std::mutex& m){ std::lock_guard g(m); return v; },
            [](int& v, std::mutex& m, int k){ std::lock_guard g(m); v = k; });
    }

    /* (B) shared mutex */
    {
        std::shared_mutex sm;
        int v = 0;
        bench("(B) shared_mtx ", v, sm,
            [](int& v, std::shared_mutex& m){ std::shared_lock g(m); return v; },
            [](int& v, std::shared_mutex& m, int k){ std::unique_lock g(m); v = k; });
    }

    /* (C) atomic */
    {
        std::atomic<int> a{0};
        std::mutex dummy;
        bench("(C) atomic     ", a, dummy,
            [](std::atomic<int>& v, std::mutex&){ return v.load(std::memory_order_relaxed); },
            [](std::atomic<int>& v, std::mutex&, int k){ v.store(k, std::memory_order_relaxed); });
    }
}
