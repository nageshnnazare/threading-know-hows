// 05_fences.cpp -- std::atomic_thread_fence: standalone barriers.
//
//  Sometimes you want the ORDERING semantics of acquire/release without
//  attaching it to a specific atomic operation. A fence applies the
//  ordering to any preceding/following memory operations.
//
//      std::atomic_thread_fence(std::memory_order_release);
//      flag.store(1, std::memory_order_relaxed);
//
//      // ... matching consumer ...
//      while (flag.load(std::memory_order_relaxed) != 1) ;
//      std::atomic_thread_fence(std::memory_order_acquire);
//      // safe to read shared state here
//
//  Equivalent to release/acquire on the flag, but if you have many flags
//  pointing to the same protected region, one fence is cheaper than
//  many acq/rel ops.
//
//  When you want SEQUENTIAL CONSISTENCY across non-atomic stores, use
//  std::memory_order_seq_cst on the fence.
//
//  Modern advice: prefer to attach orderings to specific atomics. Fences
//  are useful for low-level lock implementations.

#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>

int data;
std::atomic<int> flag{0};

void producer() {
    data = 99;
    std::atomic_thread_fence(std::memory_order_release);
    flag.store(1, std::memory_order_relaxed);
}

void consumer() {
    while (flag.load(std::memory_order_relaxed) == 0) ;
    std::atomic_thread_fence(std::memory_order_acquire);
    assert(data == 99);
    std::cout << "consumer saw data=" << data << "\n";
}

int main() {
    std::thread c(consumer), p(producer);
    p.join(); c.join();
}
