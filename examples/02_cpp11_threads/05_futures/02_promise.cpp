// 02_promise.cpp -- std::promise / std::future: a one-shot communication channel.
//
//  Imagine an empty mailbox (`promise`) with one delivery slot. The recipient
//  is given a key to the box (`future`). The sender drops a value (or an
//  exception) into the mailbox once. The recipient blocks until something
//  arrives, then opens the box exactly once.
//
//      sender (worker)                receiver (main)
//      ---------------                ----------------
//      promise<int> p;
//      future<int> f = p.get_future();    --shared state-->   f.get() blocks
//      ...
//      p.set_value(42);                                       f.get() == 42
//
//  Or:
//      p.set_exception(std::current_exception());             f.get() throws
//
//  Useful when:
//   - You manage your own thread (std::async too rigid for your use case).
//   - You want to deliver a result from the middle of an algorithm.

#include <future>
#include <iostream>
#include <thread>
#include <chrono>
#include <stdexcept>

void compute(std::promise<int> p, int x) noexcept
{
    try {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (x < 0) throw std::runtime_error("negative");
        p.set_value(x * x);
    } catch (...) {
        p.set_exception(std::current_exception());
    }
}

int main()
{
    {   // happy path
        std::promise<int> p;
        auto f = p.get_future();
        std::thread t(compute, std::move(p), 9);
        std::cout << "result = " << f.get() << "\n";
        t.join();
    }

    {   // exception path
        std::promise<int> p;
        auto f = p.get_future();
        std::thread t(compute, std::move(p), -1);
        try { f.get(); } catch (const std::exception& e) {
            std::cout << "caught: " << e.what() << "\n";
        }
        t.join();
    }
}
