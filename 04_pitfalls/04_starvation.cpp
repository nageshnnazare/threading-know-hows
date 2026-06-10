// 04_starvation.cpp -- Threads that never get a turn.
//
//  Forms of starvation:
//      * One thread holds a lock for very long stretches; others never
//        win the lock acquisition race.
//      * High-priority threads keep arriving and never let lower-priority
//        threads run (priority inversion variant).
//      * Reader-heavy load on a reader-preferring rwlock starves writers.
//
//  This file shows reader-starvation potential with shared_mutex when
//  readers are constant: the writer waits a long time. The cure on Linux
//  is that std::shared_mutex is typically writer-preferring, but that's
//  not guaranteed. The PORTABLE cure is to gate readers with a fairness
//  flag: when a writer wants in, no NEW readers may enter, but existing
//  ones drain.
//
//  Picture:
//
//      time --->
//      readers (constant): RRRRRRRRRRRRRRRRRRRRRRR...
//      writer (waiting):                           ^ when?
//
//      With fairness:
//      readers:           RRRRR  (drain)        RRRRR
//      writer waiting     [signaled "want in"] WWW
//      new readers        ...... (blocked while writer waits) .....

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

class FairRWLock {
    std::shared_mutex sm_;
    std::atomic<int>  writer_pending_{0};
public:
    void lock_shared() {
        // Polite reader: if a writer is waiting, yield and wait.
        while (writer_pending_.load(std::memory_order_acquire) > 0)
            std::this_thread::yield();
        sm_.lock_shared();
    }
    void unlock_shared() { sm_.unlock_shared(); }

    void lock() {
        writer_pending_.fetch_add(1, std::memory_order_release);
        sm_.lock();
    }
    void unlock() {
        writer_pending_.fetch_sub(1, std::memory_order_release);
        sm_.unlock();
    }
};

FairRWLock     rw;
std::atomic<int> reader_iters{0}, writer_iters{0};
std::atomic<bool> stop{false};

void reader()
{
    while (!stop.load(std::memory_order_relaxed)) {
        rw.lock_shared();
        std::this_thread::sleep_for(std::chrono::microseconds(200));
        rw.unlock_shared();
        reader_iters.fetch_add(1, std::memory_order_relaxed);
    }
}

void writer()
{
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        rw.lock();
        std::cout << "writer in (write #" << i << ")\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        rw.unlock();
        writer_iters.fetch_add(1, std::memory_order_relaxed);
    }
    stop.store(true);
}

int main()
{
    std::vector<std::thread> rs;
    for (int i = 0; i < 8; ++i) rs.emplace_back(reader);
    std::thread w(writer);
    w.join();
    for (auto& t : rs) t.join();
    std::cout << "reader_iters=" << reader_iters.load()
              << " writer_iters=" << writer_iters.load() << "\n";
}
