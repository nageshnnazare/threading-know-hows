// 05_active_object.cpp -- Encapsulate state behind a private worker thread.
//
//  Idea: instead of exposing methods that grab a mutex, the object owns
//  ONE thread that is the SOLE accessor of its state. Public methods
//  enqueue messages (lambdas) onto an internal queue. The thread executes
//  them one at a time. No mutex needed for the state itself.
//
//      caller             active object
//      ------             -------------
//      obj.add(5) ------> [worker] thread
//      obj.add(10) ----->   queue: [add(5), add(10), get]
//      auto f = obj.get()->[worker pops & runs each]
//
//  Benefits:
//      - State serialized by the queue ordering.
//      - Easy to reason: looks like a single-threaded actor.
//
//  Drawbacks:
//      - One thread per object can be expensive (use a shared scheduler).
//      - Latency = queue depth.

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <future>
#include <iostream>
#include <functional>

class Account {
    long balance_ = 0;            // accessed ONLY by worker_
    std::queue<std::function<void()>> q_;
    std::mutex                m_;
    std::condition_variable   cv_;
    bool                      done_ = false;
    std::thread               worker_;

    void run() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock g(m_);
                cv_.wait(g, [&]{ return done_ || !q_.empty(); });
                if (done_ && q_.empty()) return;
                job = std::move(q_.front()); q_.pop();
            }
            job();
        }
    }
    void post(std::function<void()> f) {
        { std::lock_guard g(m_); q_.push(std::move(f)); }
        cv_.notify_one();
    }
public:
    Account() : worker_([this]{ run(); }) {}
    ~Account() {
        { std::lock_guard g(m_); done_ = true; }
        cv_.notify_all();
        worker_.join();
    }

    // Public API: returns futures. The actual work happens on the worker thread.
    std::future<void> deposit(long amount) {
        auto p = std::make_shared<std::promise<void>>();
        auto f = p->get_future();
        post([this, amount, p]{ balance_ += amount; p->set_value(); });
        return f;
    }
    std::future<bool> withdraw(long amount) {
        auto p = std::make_shared<std::promise<bool>>();
        auto f = p->get_future();
        post([this, amount, p]{
            if (balance_ >= amount) { balance_ -= amount; p->set_value(true); }
            else                    { p->set_value(false); }
        });
        return f;
    }
    std::future<long> get_balance() {
        auto p = std::make_shared<std::promise<long>>();
        auto f = p->get_future();
        post([this, p]{ p->set_value(balance_); });
        return f;
    }
};

int main()
{
    Account a;

    a.deposit(100).get();
    a.deposit(50).get();
    std::cout << "balance: " << a.get_balance().get() << "\n";

    bool ok = a.withdraw(120).get();
    std::cout << "withdraw 120 -> " << (ok?"ok":"insufficient") << "\n";
    std::cout << "balance: " << a.get_balance().get() << "\n";
}
