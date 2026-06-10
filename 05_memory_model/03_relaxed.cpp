// 03_relaxed.cpp -- memory_order_relaxed: atomicity, NO ordering.
//
//  Useful when:
//      - You only need atomic counters (statistics, hit/miss, refcounts).
//      - You DON'T use the value to coordinate access to OTHER data.
//
//  Counter-example: do NOT use relaxed to publish data through a flag.
//      data = 42;
//      flag.store(true, relaxed);     // observer may see flag=true but data=junk
//
//  Picture (statistics counters):
//
//      threads ---atomic_fetch_add(relaxed)---> [counter]
//                  (no fence emitted on most CPUs)
//
//  On x86, relaxed ops are extremely cheap; even on ARM they're cheaper
//  than acquire/release.

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

std::atomic<long> hits{0}, misses{0};

int main()
{
    std::vector<std::thread> ts;
    for (int i = 0; i < 8; ++i)
        ts.emplace_back([&]{
            for (int k = 0; k < 1'000'000; ++k) {
                if (k % 4 == 0)
                    hits.fetch_add(1, std::memory_order_relaxed);
                else
                    misses.fetch_add(1, std::memory_order_relaxed);
            }
        });
    for (auto& t : ts) t.join();

    std::cout << "hits=" << hits.load(std::memory_order_relaxed)
              << " misses=" << misses.load(std::memory_order_relaxed) << "\n";
}
