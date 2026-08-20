// 01_thread_pool.cpp -- A reusable thread pool.
//
//  A thread pool keeps N worker threads alive, each pulling tasks from a
//  shared queue. Submitting tasks is cheap (no thread creation per task).
//
//      submit() -- enqueue + notify --> +-------+
//                                       | queue |
//                                       +-------+
//                                       /   |   \
//                                      v    v    v
//                                 worker1 worker2 worker3
//                                  pop +run  pop+run pop+run
//
//  Submitting: returns a future for the result so callers can wait/get.
//
//  Shutdown: set a stop flag, wake everyone, join. Pending tasks finish
//  unless we explicitly drop them.
//
//  Things to consider in production:
//      - Bounded queue (back-pressure on submit).
//      - Per-worker queues + work stealing for cache locality.
//      - Affinity, priority, NUMA.
//      - Task cancellation tokens.

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <future>
#include <functional>
#include <atomic>
#include <vector>
#include <memory>
#include <chrono>

class ThreadPool {
public:
    explicit ThreadPool(size_t n = std::thread::hardware_concurrency()) {
        if (n == 0) n = 2;
        for (size_t i = 0; i < n; ++i)
            workers_.emplace_back([this, i]{ worker_loop(i); });
    }

    ~ThreadPool() {
        {
            std::lock_guard g(mu_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_) w.join();
    }

    // Submit any callable; returns a future for its result.
    template <class F, class... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using R = std::invoke_result_t<F, Args...>;
        auto pkg = std::make_shared<std::packaged_task<R()>>(
            [fn = std::forward<F>(f),
             tup = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                return std::apply(fn, std::move(tup));
            });
        std::future<R> fut = pkg->get_future();
        {
            std::lock_guard g(mu_);
            if (stop_) throw std::runtime_error("pool shut down");
            tasks_.emplace([pkg]{ (*pkg)(); });
        }
        cv_.notify_one();
        return fut;
    }

    size_t size() const { return workers_.size(); }

private:
    void worker_loop(size_t /*id*/) {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock g(mu_);
                cv_.wait(g, [&]{ return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return;
                job = std::move(tasks_.front());
                tasks_.pop();
            }
            job();
        }
    }

    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex                        mu_;
    std::condition_variable           cv_;
    bool                              stop_ = false;
};

// ----------------- demo -----------------------------------------

int slow_square(int x)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return x * x;
}

int main()
{
    ThreadPool pool(4);
    std::vector<std::future<int>> futs;
    for (int i = 0; i < 12; ++i)
        futs.push_back(pool.submit(slow_square, i));
    for (size_t i = 0; i < futs.size(); ++i)
        std::cout << "f(" << i << ") = " << futs[i].get() << "\n";
}
