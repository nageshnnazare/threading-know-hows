// 03_join_detach.cpp -- Join vs detach in C++.
//
//  Three states a std::thread can be in:
//
//      JOINABLE     (just-created or running, not yet joined or detached)
//          ^                |             |
//          | join() returns |   detach()  | join()
//          |                v             v
//      NOT-JOINABLE      DETACHED      JOINED
//
//  RAII helpers:
//
//      Pre-C++20:   write your own scope_guard or use std::unique_ptr trick
//      C++20+   :   std::jthread joins automatically in its destructor.
//
//  This file shows:
//   1. A naive joinable thread (manual join).
//   2. A scope guard that joins on scope exit (exception safe).
//   3. A detached thread.

#include <iostream>
#include <thread>
#include <chrono>
#include <stdexcept>

class joiner {              // simple scope guard
    std::thread t_;
public:
    explicit joiner(std::thread t) : t_(std::move(t)) {}
    ~joiner() { if (t_.joinable()) t_.join(); }
    joiner(const joiner&) = delete;
    joiner& operator=(const joiner&) = delete;
};

void task(const char* name, int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    std::cout << "    [" << name << "] done\n";
}

int main()
{
    // 1. Plain join.
    {
        std::thread t(task, "plain", 200);
        t.join();
    }

    // 2. Exception-safe via RAII (joins even if main throws).
    try {
        joiner g(std::thread(task, "raii", 200));
        // ... if we threw here, g's destructor would still join.
        // throw std::runtime_error("oops");
    } catch (...) {}

    // 3. Detached -- fire and forget.
    std::thread d(task, "detached", 100);
    d.detach();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::cout << "[main] exiting\n";
    return 0;
}
