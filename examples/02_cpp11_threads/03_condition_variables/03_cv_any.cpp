// 03_cv_any.cpp -- std::condition_variable_any.
//
//  std::condition_variable can ONLY use std::unique_lock<std::mutex>.
//  std::condition_variable_any can use ANY Lockable type (e.g.,
//  std::shared_lock<std::shared_mutex>, your own custom RAII lock).
//
//      Use case: you have a shared_mutex protecting the data and want
//      writers to wait under exclusive lock, readers under shared.
//
//  Trade-off:
//      cv          -- faster, but only std::mutex
//      cv_any      -- slightly slower, but works with any lock

#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

std::shared_mutex             rw;
std::condition_variable_any   cv;
bool                          go = false;

int main()
{
    std::thread waiter([&]{
        std::shared_lock g(rw);                 // shared lock!
        cv.wait(g, []{ return go; });
        std::cout << "[waiter] proceed (under shared lock)\n";
    });

    std::this_thread::sleep_for(200ms);
    {
        std::unique_lock g(rw);
        go = true;
    }
    cv.notify_one();
    waiter.join();
}
