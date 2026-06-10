// 05_false_sharing.cpp -- The silent scalability killer.
//
//  CPU caches operate at CACHE LINE granularity (typically 64 bytes).
//  When two threads write to DIFFERENT variables that happen to live on
//  the SAME cache line, every write invalidates the other CPU's copy of
//  that line, causing massive cache-coherency traffic.
//
//      Memory:
//        +-----------------------+ <- one 64-byte cache line
//        | a (T0's counter)      |
//        | b (T1's counter)      |
//        | c (T2's counter)      |
//        | d (T3's counter)      |
//        +-----------------------+
//
//        Every write to `a` evicts `b,c,d` from the other cores' caches!
//        Despite NO logical sharing, performance collapses.
//
//  Cure: pad/align so each "private" variable lives on its OWN cache line.
//
//      alignas(64) struct Slot { long counter; char pad[64-sizeof(long)]; };
//      Slot s[N];     // each s[i] on its own line -> no false sharing
//
//  Run this and observe the difference (often 5-20x speedup).

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

constexpr int N = 4;
constexpr int ITERS = 50'000'000;

struct Bad {
    std::atomic<long> c[N];                       // packed; very prone to FS
};

#ifdef __cpp_lib_hardware_interference_size
constexpr size_t CL = std::hardware_destructive_interference_size;
#else
constexpr size_t CL = 64;
#endif

struct alignas(CL) Padded {
    std::atomic<long> counter;
    char pad[CL - sizeof(std::atomic<long>)];     // pad up to a full line
};

template <class Run>
double bench(const char* name, Run run) {
    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    for (int i = 0; i < N; ++i) ts.emplace_back(run, i);
    for (auto& t : ts) t.join();
    auto dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::cout << name << ": " << dt << " s\n";
    return dt;
}

int main()
{
    Bad bad{};
    bench("bad  (false-shared)", [&](int id){
        for (int k = 0; k < ITERS; ++k)
            bad.c[id].fetch_add(1, std::memory_order_relaxed);
    });

    Padded padded[N]{};
    bench("good (padded)      ", [&](int id){
        for (int k = 0; k < ITERS; ++k)
            padded[id].counter.fetch_add(1, std::memory_order_relaxed);
    });
}
