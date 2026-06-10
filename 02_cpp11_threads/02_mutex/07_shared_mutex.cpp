// 07_shared_mutex.cpp -- C++17 std::shared_mutex (read/write lock).
//
//  Two locking modes:
//
//      EXCLUSIVE (writer)  -> std::unique_lock<std::shared_mutex> g(m);
//      SHARED    (reader)  -> std::shared_lock<std::shared_mutex> g(m);
//
//  Many readers OR one writer (never both).
//
//      m state            unique_lock        shared_lock
//      -------            ------------       ------------
//      free               immediately        immediately
//      shared (n)         block              acquire (n+1 readers)
//      exclusive          block              block
//
//  Use this for read-heavy data structures (caches, configs, tables).
//
//  Trade-offs:
//      + Reads scale to many cores.
//      - Higher overhead than plain mutex per acquire.
//      - Risk of writer starvation if reads are constant.
//
//  C++14 also has std::shared_timed_mutex (with try_lock_for variants).

#include <iostream>
#include <shared_mutex>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <string>

using namespace std::chrono_literals;

class Config {
    mutable std::shared_mutex      m_;
    std::map<std::string, std::string> kv_;
public:
    void set(std::string k, std::string v) {
        std::unique_lock g(m_);
        kv_[std::move(k)] = std::move(v);
    }
    std::string get(const std::string& k) const {
        std::shared_lock g(m_);            // cheap: parallel reads
        auto it = kv_.find(k);
        return it != kv_.end() ? it->second : "";
    }
};

int main()
{
    Config c;
    c.set("greeting", "hi");

    std::vector<std::thread> ts;
    // 6 readers
    for (int i = 0; i < 6; ++i)
        ts.emplace_back([&, i]{
            for (int k = 0; k < 5; ++k) {
                std::cout << "    reader " << i << ": " << c.get("greeting") << "\n";
                std::this_thread::sleep_for(20ms);
            }
        });
    // 1 writer
    ts.emplace_back([&]{
        for (int k = 0; k < 3; ++k) {
            std::this_thread::sleep_for(50ms);
            c.set("greeting", "hi #" + std::to_string(k));
            std::cout << "[writer] updated\n";
        }
    });

    for (auto& t : ts) t.join();
}
