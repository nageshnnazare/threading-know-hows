// 01_async.cpp -- std::async: easy "run-this-elsewhere-and-give-me-result".
//
//      auto fut = std::async(launch_policy, callable, args...);
//      ... do other work ...
//      auto result = fut.get();    // blocks until ready, throws if work threw
//
//  Launch policies:
//
//      std::launch::async      -- run on a NEW thread (true parallelism)
//      std::launch::deferred   -- DON'T run yet; run on .get() in caller
//      default (both)          -- impl-defined; may not actually run async!
//
//  ALWAYS pass std::launch::async explicitly if you want a separate thread.
//
//  Picture:
//
//      main                              worker (auto thread)
//      ----                              -------
//      auto f = async(slow_fn, x); ----> compute slow_fn(x)
//      do_other_work();
//      auto r = f.get();         <----  return value to f
//
//  Future also propagates EXCEPTIONS: if the callable throws, .get() throws.

#include <future>
#include <iostream>
#include <chrono>
#include <stdexcept>

int slow(int x)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (x < 0) throw std::invalid_argument("negative not allowed");
    return x * x;
}

int main()
{
    // Run async, do other work, then collect.
    auto fut = std::async(std::launch::async, slow, 7);
    std::cout << "[main] doing other work...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "[main] result = " << fut.get() << "\n";

    // Exception propagation:
    try {
        auto bad = std::async(std::launch::async, slow, -1);
        bad.get();
    } catch (const std::exception& e) {
        std::cout << "[main] caught: " << e.what() << "\n";
    }

    // Polling with future::wait_for:
    auto f2 = std::async(std::launch::async, slow, 11);
    while (f2.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready)
        std::cout << "[main] waiting...\n";
    std::cout << "[main] f2 = " << f2.get() << "\n";
}
