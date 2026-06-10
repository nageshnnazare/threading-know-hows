// 01_basic_mutex.cpp -- std::mutex with manual lock/unlock (DON'T do this).
//
//  std::mutex offers .lock(), .try_lock(), .unlock() -- but using them
//  directly is error-prone (forget to unlock on exception -> deadlock).
//
//  This file SHOWS the manual API for completeness, then directs you
//  to lock_guard/unique_lock for production code.
//
//  +--------- std::mutex life cycle -----------+
//  |   default-constructed (unlocked)          |
//  |       |                                   |
//  |       v                                   |
//  |   .lock() -- blocks until acquired        |
//  |       |                                   |
//  |       v                                   |
//  |   in critical section                     |
//  |       |                                   |
//  |       v                                   |
//  |   .unlock()                               |
//  +-------------------------------------------+

#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

static std::mutex g_lock;
static long       counter = 0;

static void worker_manual(int n)
{
    for (int i = 0; i < n; ++i) {
        g_lock.lock();
        ++counter;            // CRITICAL: if we throw between lock() and unlock(),
        g_lock.unlock();      // the mutex stays locked forever.
    }
}

int main()
{
    constexpr int N = 8, ITERS = 100000;
    std::vector<std::thread> ts;
    ts.reserve(N);
    for (int i = 0; i < N; ++i) ts.emplace_back(worker_manual, ITERS);
    for (auto& t : ts) t.join();
    std::cout << "counter=" << counter << " (expected " << N*ITERS << ")\n";
    std::cout << "(prefer lock_guard / scoped_lock; see next examples)\n";
    return 0;
}
