// 02_producer_consumer.cpp -- Bounded thread-safe queue.
//
//  Compared with the C version, this is far cleaner:
//      - templated on element type
//      - RAII locks
//      - lambda predicates
//      - "shutdown" signaling via a sentinel bool
//
//  Picture (capacity 4):
//
//      head                       tail
//       |                          |
//       v                          v
//      [a][b][c][d]                       <-- count = 4 (FULL)
//
//      after pop:
//          [b][c][d]_                     <-- count = 3
//
//      Two predicates:
//          push waits while count == cap
//          pop  waits while count == 0 (and !done)

#include <condition_variable>
#include <mutex>
#include <thread>
#include <queue>
#include <iostream>
#include <vector>
#include <chrono>
#include <optional>

using namespace std::chrono_literals;

template <class T>
class BoundedQueue {
    std::mutex                   m_;
    std::condition_variable      not_full_, not_empty_;
    std::queue<T>                q_;
    size_t                       cap_;
    bool                         done_ = false;
public:
    explicit BoundedQueue(size_t cap) : cap_(cap) {}

    void push(T v) {
        std::unique_lock g(m_);
        not_full_.wait(g, [&]{ return q_.size() < cap_ || done_; });
        if (done_) return;
        q_.push(std::move(v));
        g.unlock();
        not_empty_.notify_one();
    }

    std::optional<T> pop() {
        std::unique_lock g(m_);
        not_empty_.wait(g, [&]{ return !q_.empty() || done_; });
        if (q_.empty()) return std::nullopt;        // closed and drained
        T v = std::move(q_.front());
        q_.pop();
        g.unlock();
        not_full_.notify_one();
        return v;
    }

    void close() {
        {
            std::lock_guard g(m_);
            done_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }
};

int main()
{
    BoundedQueue<int> q(4);

    std::thread producer([&]{
        for (int i = 0; i < 20; ++i) {
            q.push(i);
            std::cout << "[P] pushed " << i << "\n";
            std::this_thread::sleep_for(30ms);
        }
        q.close();
    });

    std::vector<std::thread> consumers;
    for (int c = 0; c < 3; ++c) {
        consumers.emplace_back([&, c]{
            while (auto v = q.pop()) {
                std::cout << "    [C" << c << "] got " << *v << "\n";
                std::this_thread::sleep_for(80ms);
            }
            std::cout << "    [C" << c << "] done\n";
        });
    }

    producer.join();
    for (auto& t : consumers) t.join();
}
