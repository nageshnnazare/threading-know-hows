// 02_producer_consumer.cpp -- Multi-producer / multi-consumer pattern with
//                              a sentinel-based clean shutdown.
//
//  Same shape as the cv example but adds a "shutdown" mechanism and shows
//  how multiple producers AND multiple consumers coordinate cleanly.
//
//      producers --push-->  bounded queue  <--pop-- consumers
//
//  When all producers are done, the LAST producer calls queue.close()
//  which causes all subsequent pops to return nullopt once drained.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

template <class T>
class Queue {
    std::mutex                  m_;
    std::condition_variable     cv_full_, cv_empty_;
    std::queue<T>               q_;
    size_t                      cap_;
    bool                        closed_ = false;
public:
    explicit Queue(size_t cap) : cap_(cap) {}

    bool push(T v) {
        std::unique_lock g(m_);
        cv_full_.wait(g, [&]{ return closed_ || q_.size() < cap_; });
        if (closed_) return false;
        q_.push(std::move(v));
        cv_empty_.notify_one();
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock g(m_);
        cv_empty_.wait(g, [&]{ return closed_ || !q_.empty(); });
        if (q_.empty()) return std::nullopt;
        T v = std::move(q_.front()); q_.pop();
        cv_full_.notify_one();
        return v;
    }

    void close() {
        { std::lock_guard g(m_); closed_ = true; }
        cv_empty_.notify_all();
        cv_full_.notify_all();
    }
};

int main()
{
    constexpr int NP = 3, NC = 4, ITEMS_PER_P = 10;
    Queue<int> q(8);
    std::atomic<int> produced{0}, consumed{0};
    std::atomic<int> producers_done{0};

    std::vector<std::thread> producers, consumers;

    for (int p = 0; p < NP; ++p)
        producers.emplace_back([&, p]{
            for (int i = 0; i < ITEMS_PER_P; ++i) {
                q.push(p * 1000 + i);
                produced.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            if (producers_done.fetch_add(1) + 1 == NP)
                q.close();                        // last one closes the queue
        });

    for (int c = 0; c < NC; ++c)
        consumers.emplace_back([&, c]{
            while (auto v = q.pop()) {
                std::cout << "    [C" << c << "] " << *v << "\n";
                consumed.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    std::cout << "produced " << produced << " consumed " << consumed << "\n";
}
