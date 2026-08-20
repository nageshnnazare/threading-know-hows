// 02_atomic_flag.cpp -- The lowest-level atomic primitive.
//
//  std::atomic_flag is THE only atomic type guaranteed to be lock-free
//  on every conforming platform. It exposes only:
//
//      test_and_set()  -- set to true, return previous value
//      clear()         -- set to false
//      test()          -- (C++20) read without modify
//
//  Classic spinlock implemented with it:
//
//      class Spinlock {
//          std::atomic_flag f = ATOMIC_FLAG_INIT;
//      public:
//          void lock()   { while (f.test_and_set(std::memory_order_acquire)) {} }
//          void unlock() { f.clear(std::memory_order_release); }
//      };
//
//  Diagram of test_and_set semantics:
//
//      thread A                 thread B
//      --------                 --------
//      t&s -> false             t&s -> true (B is already in)
//      (A wins, enters CS)      (B spins)
//      ...                      ...
//      f.clear()                t&s -> false  (B finally enters)

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

class Spinlock {
    std::atomic_flag f_ = ATOMIC_FLAG_INIT;
public:
    void lock() {
        while (f_.test_and_set(std::memory_order_acquire))
            ;       // could yield/pause here in production
    }
    void unlock() { f_.clear(std::memory_order_release); }
};

Spinlock s;
long     n = 0;

int main()
{
    constexpr int N = 8, ITERS = 100000;
    std::vector<std::thread> ts;
    for (int i = 0; i < N; ++i) {
        ts.emplace_back([]{
            for (int k = 0; k < ITERS; ++k) {
                s.lock();
                ++n;
                s.unlock();
            }
        });
    }
    for (auto& t : ts) t.join();
    std::cout << "n = " << n << " (expected " << N*ITERS << ")\n";
}
