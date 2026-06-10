// 03_packaged_task.cpp -- std::packaged_task: callable wired to a future.
//
//  A packaged_task wraps a callable so that, when invoked, its return value
//  (or exception) is stored into the linked future.
//
//      packaged_task<int(int)> task(slow);
//      future<int> fut = task.get_future();
//      thread(std::move(task), 7).detach();   // task() will fill the future
//      ...
//      int r = fut.get();
//
//  Use cases:
//      - Build a TASK QUEUE (your own thread pool) where tasks are
//        std::function<void()> that internally call .operator()() on a
//        packaged_task. The submitter holds the matching future.
//      - More flexibility than std::async (you decide where & when to
//        invoke).

#include <future>
#include <thread>
#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>
#include <chrono>

class TinyPool {
    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> q_;
    std::mutex                        m_;
    std::condition_variable           cv_;
    bool                              stop_ = false;
public:
    explicit TinyPool(size_t n) {
        for (size_t i = 0; i < n; ++i)
            workers_.emplace_back([this]{
                for (;;) {
                    std::function<void()> job;
                    {
                        std::unique_lock g(m_);
                        cv_.wait(g, [&]{ return stop_ || !q_.empty(); });
                        if (stop_ && q_.empty()) return;
                        job = std::move(q_.front()); q_.pop();
                    }
                    job();
                }
            });
    }
    ~TinyPool() {
        { std::lock_guard g(m_); stop_ = true; }
        cv_.notify_all();
        for (auto& w : workers_) w.join();
    }
    template <class F, class... A>
    auto submit(F&& f, A&&... a) -> std::future<std::invoke_result_t<F, A...>> {
        using R = std::invoke_result_t<F, A...>;
        auto task = std::make_shared<std::packaged_task<R()>>(
            [f = std::forward<F>(f),
             tup = std::make_tuple(std::forward<A>(a)...)]() mutable {
                return std::apply(f, std::move(tup));
            });
        std::future<R> fut = task->get_future();
        {
            std::lock_guard g(m_);
            q_.emplace([task]{ (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }
};

int main()
{
    TinyPool pool(4);
    std::vector<std::future<int>> futs;
    for (int i = 0; i < 8; ++i)
        futs.push_back(pool.submit([](int x){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return x * x;
        }, i));
    for (auto& f : futs) std::cout << f.get() << " ";
    std::cout << "\n";
}
