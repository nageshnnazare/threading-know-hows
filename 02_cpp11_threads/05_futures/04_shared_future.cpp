// 04_shared_future.cpp -- One result, many readers.
//
//  std::future is single-consumer: only ONE thread can call .get(). If you
//  need multiple consumers to read the same result, use std::shared_future.
//
//      std::future<int>        f = ...;
//      std::shared_future<int> sf = f.share();    // sf is COPYABLE
//
//      // Many threads:
//      sf.get();   // each thread can call get()
//
//  Picture:
//
//      promise --once--> shared_future
//                              |
//                +-------------+-------------+
//                |             |             |
//             reader1       reader2       reader3
//             .get()        .get()        .get()
//             (all see the same result, no copy of the actual T)

#include <future>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

int main()
{
    std::promise<int> p;
    std::shared_future<int> sf = p.get_future().share();

    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([sf, i]{           // sf is COPIED -> ok
            int v = sf.get();
            std::cout << "  reader " << i << " got " << v << "\n";
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    p.set_value(123);
    for (auto& t : readers) t.join();
}
