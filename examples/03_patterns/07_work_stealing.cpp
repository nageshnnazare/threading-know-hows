// 07_work_stealing.cpp -- Per-worker deques + steal-from-others.
//
//  When tasks are unevenly distributed, idle workers can STEAL work from
//  other workers' queues. Used by TBB, Cilk, Go scheduler, ForkJoin in JVM.
//
//      Each worker pushes/pops from its own deque (LIFO from its end -> warm cache):
//
//          Worker 0 deque: [t0, t1, t2, t3]
//          Worker 1 deque: [t4, t5]
//          Worker 2 deque: []
//
//      Worker 2 has nothing to do; steals from Worker 0's OTHER end (FIFO):
//
//          Worker 0 deque: [t1, t2, t3]
//          Worker 2: now executing t0
//
//  Why steal from the OPPOSITE end?
//      - Owner pops from one end (hot tasks with cache locality).
//      - Thieves pop from the other end (older tasks, cold anyway).
//      - Reduces contention.
//
//  This file is a simplified, MUTEX-PROTECTED deque pool (not lock-free, but
//  illustrates the idea). Production work-stealing uses Chase-Lev deques.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

class WorkStealingPool {
    using Task = std::function<void()>;

    struct WorkerData {
        std::deque<Task> dq;
        std::mutex       m;
    };

    std::vector<std::unique_ptr<WorkerData>> data_;
    std::vector<std::thread>                 workers_;
    std::atomic<bool>                        stop_{false};
    std::atomic<int>                         pending_{0};

    Task pop_local(int who) {
        auto& d = *data_[who];
        std::lock_guard g(d.m);
        if (d.dq.empty()) return {};
        Task t = std::move(d.dq.back());           // LIFO from back
        d.dq.pop_back();
        return t;
    }
    Task steal_from(int victim) {
        auto& d = *data_[victim];
        std::lock_guard g(d.m);
        if (d.dq.empty()) return {};
        Task t = std::move(d.dq.front());          // FIFO from front
        d.dq.pop_front();
        return t;
    }

    void worker_loop(int id) {
        std::mt19937 rng(id);
        while (!stop_.load()) {
            Task t = pop_local(id);
            if (!t) {
                // Try to steal.
                for (size_t k = 0; k < data_.size(); ++k) {
                    int victim = rng() % int(data_.size());
                    if (victim == id) continue;
                    t = steal_from(victim);
                    if (t) break;
                }
            }
            if (t) { t(); pending_.fetch_sub(1); }
            else if (pending_.load() == 0 && stop_.load()) break;
            else std::this_thread::yield();
        }
    }

public:
    explicit WorkStealingPool(size_t n) {
        for (size_t i = 0; i < n; ++i) data_.emplace_back(std::make_unique<WorkerData>());
        for (size_t i = 0; i < n; ++i)
            workers_.emplace_back([this, i]{ worker_loop(int(i)); });
    }
    ~WorkStealingPool() {
        // Drain
        while (pending_.load() > 0) std::this_thread::yield();
        stop_.store(true);
        for (auto& w : workers_) w.join();
    }

    // Submit to round-robin worker for demo.
    void submit(Task t) {
        static std::atomic<int> rr{0};
        int who = rr.fetch_add(1) % int(data_.size());
        {
            std::lock_guard g(data_[who]->m);
            data_[who]->dq.push_back(std::move(t));
        }
        pending_.fetch_add(1);
    }
};

int main()
{
    WorkStealingPool pool(4);

    std::atomic<long> sum{0};
    for (int i = 1; i <= 200; ++i)
        pool.submit([&, i]{
            std::this_thread::sleep_for(std::chrono::milliseconds(i % 5));
            sum.fetch_add(i, std::memory_order_relaxed);
        });

    // Wait by destructing the pool.
    // (In real code, expose a wait() method.)
    {
        // Force pool to flush before printing
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            // Crude completion check: read from internal pending via friend in real code.
            // Here we just sleep enough.
            break;
        }
    }
    // Sleep enough for all tasks to finish.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "sum=" << sum.load()
              << " expected=" << (200 * 201 / 2) << "\n";
}
