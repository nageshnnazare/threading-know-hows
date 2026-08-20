// 06_atomic_pointer.cpp -- atomic<T*> for "publish a snapshot" pattern.
//
//  Common scenario: you have read-mostly data updated occasionally. Instead
//  of locking on every read, build a NEW snapshot and atomically swap the
//  pointer. Readers see either the old or the new -- never a torn state.
//
//      readers       atomic<Snapshot*> ptr      writer
//      -------                                  ------
//      load(acquire) ----.                       build New
//                         \                       store(release, New)
//                          v
//                    [old snap, immutable]
//                    [new snap, immutable]
//
//  Memory reclamation again: when is it safe to free the old snapshot?
//  Patterns: RCU, hazard pointers, shared_ptr<Atomic> (C++20), or simply
//  "wait until no readers can possibly hold a pointer to the old snap".
//  Below we LEAK on every update to keep the example simple.

#include <atomic>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

using namespace std::chrono_literals;

struct Config {
    int max_connections;
    int timeout_ms;
};

std::atomic<Config*> g_cfg;

int main()
{
    g_cfg.store(new Config{100, 5000}, std::memory_order_release);

    std::atomic<bool> stop{false};

    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i)
        readers.emplace_back([&]{
            while (!stop.load(std::memory_order_relaxed)) {
                Config* c = g_cfg.load(std::memory_order_acquire);
                std::cout << "  reader sees max=" << c->max_connections
                          << " timeout=" << c->timeout_ms << "\n";
                std::this_thread::sleep_for(50ms);
            }
        });

    std::thread writer([&]{
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(120ms);
            Config* fresh = new Config{100 + i*10, 5000 - i*100};
            g_cfg.store(fresh, std::memory_order_release);
            // OLD pointer is leaked here.
        }
    });

    writer.join();
    stop.store(true, std::memory_order_relaxed);
    for (auto& t : readers) t.join();
}
