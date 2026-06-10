// 01_atomic_basic.cpp -- std::atomic<T>: concurrency without locks.
//
//  std::atomic<T> guarantees:
//      - INDIVISIBILITY: read or write of the atomic is "all-or-nothing"
//      - VISIBILITY:     stores become visible to other threads (under
//                        the requested memory order, see next examples)
//
//  Common atomic types:
//      std::atomic<int>, std::atomic<bool>, std::atomic<size_t>,
//      std::atomic<T*>, std::atomic_flag, std::atomic<uint64_t>, ...
//      For arbitrary T, std::atomic<T> works if T is trivially copyable
//      AND <= a hardware-supported size (often 8 bytes); otherwise the
//      implementation falls back to a hidden lock (`is_lock_free()` -> false).
//
//  Operations (common):
//      a.load([order])                     read
//      a.store(v, [order])                 write
//      a.exchange(v, [order])              swap
//      a.compare_exchange_weak(expected, desired, [order])
//      a.compare_exchange_strong(...)
//      a.fetch_add/sub/and/or/xor          atomic RMW (numeric/integer)
//      ++a / a++                           shorthand for fetch_add
//
//  COUNTER EXAMPLE:
//
//      Without atomic:    ++counter -> race -> wrong total
//      With atomic:       ++counter -> CPU emits LOCK XADD (x86) -> safe

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

std::atomic<long> g_counter{0};

void worker(int n)
{
    for (int i = 0; i < n; ++i)
        ++g_counter;            // atomic increment, no mutex needed
}

int main()
{
    std::cout << "is_lock_free<long> = "
              << std::atomic<long>::is_always_lock_free << "\n";

    constexpr int N = 8, ITERS = 200000;
    std::vector<std::thread> ts;
    for (int i = 0; i < N; ++i) ts.emplace_back(worker, ITERS);
    for (auto& t : ts) t.join();

    std::cout << "counter = " << g_counter.load()
              << " (expected " << long(N) * ITERS << ")\n";
}
