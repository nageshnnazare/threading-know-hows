// 02_deadlock.cpp -- Lock-ordering inversion (and three cures).
//
//  Two threads each hold one lock and want the other:
//
//      A holds m1, wants m2 -.        circular wait
//      B holds m2, wants m1 -+----- DEADLOCK
//
//  Cures (any one breaks the cycle):
//
//   (i) Lock in a fixed GLOBAL ORDER (e.g. by address):
//          if (&m1 < &m2) lock m1 then m2; else lock m2 then m1;
//
//   (ii) Use std::lock(m1, m2) / std::scoped_lock(m1, m2) which uses
//        try-and-back-off internally and CAN'T deadlock between them.
//
//   (iii) Use try_lock with backoff on failure (release & retry).
//
//  This file demonstrates the deadlock with a 2-second alarm, then the
//  three cures. Run with -fsanitize=thread to see TSan diagnostics.

#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

std::mutex m1, m2;

[[maybe_unused]] static void deadlock_demo()
{
    std::thread a([]{
        std::lock_guard<std::mutex> g1(m1);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::lock_guard<std::mutex> g2(m2);
        std::cout << "A got both\n";
    });
    std::thread b([]{
        std::lock_guard<std::mutex> g2(m2);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::lock_guard<std::mutex> g1(m1);
        std::cout << "B got both\n";
    });
    a.join(); b.join();
}

/* (i) Global order */
static void cure_global_order()
{
    auto take_both = [](std::mutex& x, std::mutex& y){
        if (&x < &y) { x.lock(); y.lock(); }
        else         { y.lock(); x.lock(); }
        x.unlock(); y.unlock();
    };
    std::thread a([&]{ take_both(m1, m2); });
    std::thread b([&]{ take_both(m2, m1); });
    a.join(); b.join();
    std::cout << "(i)   fixed-order: ok\n";
}

/* (ii) std::scoped_lock */
static void cure_scoped_lock()
{
    auto fn = []{ std::scoped_lock g(m1, m2); };
    std::thread a(fn), b(fn);
    a.join(); b.join();
    std::cout << "(ii)  scoped_lock: ok\n";
}

/* (iii) try_lock with backoff */
static void cure_try_lock()
{
    auto take_with_backoff = []{
        for (;;) {
            std::unique_lock l1(m1, std::try_to_lock);
            if (!l1) { std::this_thread::yield(); continue; }
            std::unique_lock l2(m2, std::try_to_lock);
            if (!l2) {
                l1.unlock();
                std::this_thread::yield();
                continue;
            }
            // got both
            return;
        }
    };
    std::thread a(take_with_backoff), b(take_with_backoff);
    a.join(); b.join();
    std::cout << "(iii) try-lock: ok\n";
}

int main()
{
    cure_global_order();
    cure_scoped_lock();
    cure_try_lock();
    // To see the actual deadlock, uncomment the next two lines.
    // alarm(2);
    // deadlock_demo();
}
