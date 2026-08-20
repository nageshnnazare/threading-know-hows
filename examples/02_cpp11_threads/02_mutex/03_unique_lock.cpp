// 03_unique_lock.cpp -- The Swiss army knife of locks.
//
//  std::unique_lock is movable, can defer locking, can be released early,
//  can transfer ownership, and is the type required by condition variables
//  (cv.wait expects a unique_lock<mutex>).
//
//      Capability           lock_guard      unique_lock
//      ----------           ----------      ------------
//      RAII unlock          yes             yes
//      Move-only            no              YES (transfer ownership)
//      Defer lock           no              YES (std::defer_lock)
//      Try lock             no              YES (std::try_to_lock)
//      Adopt already-locked no              YES (std::adopt_lock)
//      Manual unlock        no              YES
//      Pass to cv.wait      no              YES
//      Cost                 minimal         tiny extra
//
//  Common idioms:
//
//      // 1. Defer-and-multi-lock to avoid deadlock:
//      std::unique_lock<std::mutex> a(m1, std::defer_lock);
//      std::unique_lock<std::mutex> b(m2, std::defer_lock);
//      std::lock(a, b);            // takes both atomically
//
//      // 2. Lock, do something, briefly release, re-lock:
//      std::unique_lock<std::mutex> g(m);
//      ...
//      g.unlock();
//      do_long_work_without_holding();
//      g.lock();
//      ...
//
//      // 3. Pass into condition_variable::wait:
//      std::unique_lock<std::mutex> g(m);
//      cv.wait(g, [&]{ return ready; });

#include <iostream>
#include <mutex>
#include <thread>
#include <chrono>

static std::mutex m1, m2;

static void atomic_dual()
{
    // Defer locking, then std::lock acquires BOTH atomically (deadlock-free).
    std::unique_lock<std::mutex> a(m1, std::defer_lock);
    std::unique_lock<std::mutex> b(m2, std::defer_lock);
    std::lock(a, b);
    std::cout << "[" << std::this_thread::get_id() << "] holds both\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

static void release_then_relock()
{
    std::unique_lock<std::mutex> g(m1);
    std::cout << "phase 1 (locked)\n";

    g.unlock();                                  // release temporarily
    std::cout << "phase 2 (no lock held; do long work)\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    g.lock();                                    // reacquire
    std::cout << "phase 3 (locked again)\n";
}

int main()
{
    std::thread t1(atomic_dual), t2(atomic_dual);
    t1.join(); t2.join();

    release_then_relock();
}
