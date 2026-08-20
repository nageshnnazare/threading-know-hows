// 01_hello_thread.cpp -- The simplest std::thread program.
//
//  Compile : g++ -std=c++17 -pthread -O2 -Wall -o 01_hello_thread 01_hello_thread.cpp
//  Run     : ./01_hello_thread
//
//  std::thread takes any callable + arguments, perfect-forwarded:
//
//        std::thread t(callable, arg1, arg2, ...);
//
//  ASCII timeline:
//
//      main thread                worker thread
//      -----------                -------------
//          |
//          | std::thread t(fn) -> spawned
//          |                          |
//          | t.join() ----- waits     | runs fn()
//          |                          |
//          |  <----- exits -----------+
//          v
//        main returns
//
//  RULES:
//   * If a std::thread object's destructor runs while it is JOINABLE,
//     std::terminate() is called. So you MUST .join() or .detach() before
//     the thread variable goes out of scope.
//   * Use std::jthread (C++20) to get automatic .join() in the destructor.

#include <iostream>
#include <thread>

void worker()
{
    std::cout << "[worker] hello from thread "
              << std::this_thread::get_id() << "\n";
}

int main()
{
    std::cout << "[main  ] id=" << std::this_thread::get_id() << "\n";
    std::cout << "[main  ] hardware_concurrency = "
              << std::thread::hardware_concurrency() << "\n";

    std::thread t(worker);
    t.join();                         // MUST join (or detach) before t dies
    return 0;
}
