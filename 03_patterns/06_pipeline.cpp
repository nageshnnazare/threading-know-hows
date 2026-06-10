// 06_pipeline.cpp -- Stages connected by bounded queues.
//
//      [source] -> q1 -> [stage1: square] -> q2 -> [stage2: stringify] -> q3 -> [sink]
//
//  Each stage is a thread. Items flow forward; back-pressure is automatic
//  because each queue is bounded.
//
//      In the source: produce items.
//      In each stage: pop, transform, push.
//      In the sink:   pop, print.
//      Shutdown:      source closes q1; each stage detects "no more input,
//                     queue drained" and closes its output queue, then exits.
//
//  Diagram:
//
//      stage1 q       stage2 q       sink q
//      +--------+     +--------+     +--------+
//      |        |     |        |     |        |
//      | source |---->| square |---->| str    |---->| sink
//      |        |     |        |     |        |     |
//      +--------+     +--------+     +--------+
//
//  Pipelines are great when work has natural stages that can run in
//  parallel and items are independent.

#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <string>
#include <chrono>

template <class T>
class BQ {
    std::queue<T> q_;
    size_t cap_;
    bool closed_ = false;
    std::mutex m_;
    std::condition_variable cv_full_, cv_empty_;
public:
    explicit BQ(size_t cap) : cap_(cap) {}
    void push(T v) {
        std::unique_lock g(m_);
        cv_full_.wait(g, [&]{ return q_.size() < cap_ || closed_; });
        if (closed_) return;
        q_.push(std::move(v));
        cv_empty_.notify_one();
    }
    std::optional<T> pop() {
        std::unique_lock g(m_);
        cv_empty_.wait(g, [&]{ return !q_.empty() || closed_; });
        if (q_.empty()) return std::nullopt;
        T v = std::move(q_.front()); q_.pop();
        cv_full_.notify_one();
        return v;
    }
    void close() {
        { std::lock_guard g(m_); closed_ = true; }
        cv_empty_.notify_all(); cv_full_.notify_all();
    }
};

int main()
{
    BQ<int>         q1(4);
    BQ<long>        q2(4);
    BQ<std::string> q3(4);

    std::thread source([&]{
        for (int i = 1; i <= 12; ++i) {
            q1.push(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        q1.close();
    });

    std::thread stage1([&]{
        while (auto v = q1.pop()) q2.push(long(*v) * (*v));
        q2.close();
    });

    std::thread stage2([&]{
        while (auto v = q2.pop()) q3.push("(" + std::to_string(*v) + ")");
        q3.close();
    });

    std::thread sink([&]{
        while (auto s = q3.pop()) std::cout << "  sink: " << *s << "\n";
    });

    source.join(); stage1.join(); stage2.join(); sink.join();
}
