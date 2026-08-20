// 01_call_once.cpp -- std::call_once / std::once_flag.
//
//  Guarantees a function is executed EXACTLY ONCE, even with many threads
//  arriving simultaneously. The first to arrive runs it; others block until
//  it completes.
//
//      std::once_flag once;
//      std::call_once(once, init_fn, args...);
//
//  Use cases:
//      - Lazy singleton init.
//      - Lazy resource open (file, db).
//      - Replace double-checked locking (which is hard to get right
//        without atomics).
//
//  Picture:
//
//      thread 1: call_once(once, init) -- runs init, then returns
//      thread 2: call_once(once, init) -- BLOCKS until thread1 finishes,
//                                         then returns without running init
//      thread 3: call_once(once, init) -- since flag is set, returns immediately
//
//  Note: in C++11+, function-local static initialization is ALSO thread-safe
//  ("Meyers singleton") and is usually simpler:
//
//      Foo& instance() {
//          static Foo f;        // standard guarantees thread-safe init
//          return f;
//      }

#include <mutex>
#include <iostream>
#include <thread>
#include <vector>

std::once_flag g_init;
std::string    g_data;

void init() {
    std::cout << "    [init] performing one-time setup\n";
    g_data = "initialized";
}

void worker(int id)
{
    std::call_once(g_init, init);
    std::cout << "[w" << id << "] saw g_data = \"" << g_data << "\"\n";
}

int main()
{
    std::vector<std::thread> ts;
    for (int i = 0; i < 8; ++i) ts.emplace_back(worker, i);
    for (auto& t : ts) t.join();
}
