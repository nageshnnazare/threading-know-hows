// 04_thread_id.cpp -- std::this_thread, std::thread::id, hardware_concurrency
//
//  Useful introspection primitives:
//
//      std::this_thread::get_id()                -- ID of current thread
//      std::this_thread::sleep_for(duration)     -- portable sleep
//      std::this_thread::sleep_until(time_point) -- portable absolute sleep
//      std::this_thread::yield()                 -- "I'm spinning, be nice"
//      std::thread::hardware_concurrency()       -- cores hint (0 if unknown)
//
//      std::thread t(...);
//      t.get_id()           -- the spawned thread's id
//      t.native_handle()    -- pthread_t/HANDLE (for OS-specific tweaks)
//
//  IDs are comparable and hashable -- use them as map keys to track threads.

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <mutex>

int main()
{
    std::cout << "hardware_concurrency = "
              << std::thread::hardware_concurrency() << "\n";
    std::cout << "main id = " << std::this_thread::get_id() << "\n";

    std::mutex m;
    std::unordered_map<std::thread::id, int> seen_count;

    std::vector<std::thread> ts;
    for (int i = 0; i < 4; ++i) {
        ts.emplace_back([&, i]{
            for (int k = 0; k < 3; ++k) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                std::lock_guard<std::mutex> g(m);
                seen_count[std::this_thread::get_id()]++;
                std::cout << "  worker " << i
                          << " (id " << std::this_thread::get_id() << ")"
                          << " iter " << k << "\n";
            }
        });
    }
    for (auto& t : ts) t.join();

    std::cout << "Saw " << seen_count.size() << " distinct ids:\n";
    for (auto& [id, n] : seen_count)
        std::cout << "    " << id << " : " << n << "\n";
    return 0;
}
