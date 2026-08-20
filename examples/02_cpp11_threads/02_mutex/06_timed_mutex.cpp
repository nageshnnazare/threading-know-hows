// 06_timed_mutex.cpp -- timed_mutex / recursive_timed_mutex.
//
//  Adds two methods over a regular mutex:
//
//      try_lock_for(duration)        wait at most this long
//      try_lock_until(time_point)    wait until this absolute time
//
//  Returns true on success, false on timeout. Combine with std::unique_lock
//  for RAII semantics.
//
//  ASCII:
//
//      time --->
//      holder:   |================================|     (5s)
//      waiter:   (try_lock_for 1s)
//                  ===timeout (false)
//                                  (try_lock_for 1s)
//                                    ===timeout
//                                                (try_lock_for 5s)
//                                                  ===acquired

#include <iostream>
#include <mutex>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

static std::timed_mutex m;

void holder()
{
    std::lock_guard<std::timed_mutex> g(m);
    std::this_thread::sleep_for(2s);
}

void waiter()
{
    std::this_thread::sleep_for(50ms);    // let holder grab first
    if (m.try_lock_for(500ms)) {
        std::cout << "[waiter] got it within 500ms\n";
        m.unlock();
    } else {
        std::cout << "[waiter] timed out after 500ms (holder still busy)\n";
    }

    if (m.try_lock_for(3s)) {
        std::cout << "[waiter] got it within 3s\n";
        m.unlock();
    } else {
        std::cout << "[waiter] timed out after 3s\n";
    }
}

int main()
{
    std::thread h(holder), w(waiter);
    h.join(); w.join();
}
