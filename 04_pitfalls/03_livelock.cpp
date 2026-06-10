// 03_livelock.cpp -- The "polite" deadlock.
//
//  Two threads each acquire one lock and try the other; if they fail, they
//  RELEASE and retry. Without backoff, they retry in lock-step forever:
//
//      A: lock m1; try m2 -> busy; release m1; retry
//      B: lock m2; try m1 -> busy; release m2; retry
//      ... ad infinitum
//
//      Picture:
//
//          A: m1   m1   m1   m1   m1   ...
//          B:   m2   m2   m2   m2   m2 ...
//          (neither ever holds both)
//
//  Cure: RANDOMIZED EXPONENTIAL BACKOFF. With a random delay between
//  retries, the lock-step pattern is broken and one thread eventually wins.
//
//  This file has both the buggy version (will eventually succeed because
//  scheduling is jittery on Linux, but with low probability per attempt)
//  and the fixed version. We bound runs with attempt counts.

#include <chrono>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>

std::mutex m1, m2;

void buggy(int id, int& iterations)
{
    std::mutex& first  = (id == 0) ? m1 : m2;
    std::mutex& second = (id == 0) ? m2 : m1;

    for (int n = 0; n < 200; ++n) {
        first.lock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (second.try_lock()) {
            std::cout << id << " got both at attempt " << n << "\n";
            second.unlock();
            first.unlock();
            iterations = n;
            return;
        }
        first.unlock();
        // No backoff -> livelock-prone
    }
    iterations = -1;
}

void fixed(int id, int& iterations)
{
    std::mutex& first  = (id == 0) ? m1 : m2;
    std::mutex& second = (id == 0) ? m2 : m1;
    std::mt19937 rng(id ^ 0xc01dbeef);

    for (int n = 0; n < 200; ++n) {
        first.lock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (second.try_lock()) {
            std::cout << id << " (fixed) got both at attempt " << n << "\n";
            second.unlock();
            first.unlock();
            iterations = n;
            return;
        }
        first.unlock();
        // Randomized backoff breaks lock-step
        std::this_thread::sleep_for(std::chrono::microseconds(50 + rng() % 500));
    }
    iterations = -1;
}

int main()
{
    int it_a = 0, it_b = 0;
    {
        std::thread a(buggy, 0, std::ref(it_a));
        std::thread b(buggy, 1, std::ref(it_b));
        a.join(); b.join();
        std::cout << "buggy   attempts: A=" << it_a << " B=" << it_b << "\n";
    }
    {
        std::thread a(fixed, 0, std::ref(it_a));
        std::thread b(fixed, 1, std::ref(it_b));
        a.join(); b.join();
        std::cout << "fixed   attempts: A=" << it_a << " B=" << it_b << "\n";
    }
}
