// 04_dining_philosophers.cpp -- The classic synchronization puzzle.
//
//      5 philosophers around a table.
//      5 forks (one between each pair).
//      A philosopher needs BOTH the fork on their left AND right to eat.
//
//          (P0)
//        F4    F0
//      (P4)     (P1)
//        F3    F1
//          (P3)----F2----(P2)
//
//  The naive "always grab left, then right" deadlocks: every philosopher
//  picks up their left fork; nobody can ever get their right fork.
//
//  CLASSIC FIX: lock-ordering. Philosopher N normally grabs (left,right),
//  but the LAST one inverts to (right,left). That breaks the cycle.
//
//  This file uses std::scoped_lock to lock both forks atomically -- a
//  modern, deadlock-free solution that doesn't require manual ordering.

#include <iostream>
#include <mutex>
#include <thread>
#include <chrono>
#include <vector>

constexpr int N = 5;

int main()
{
    std::mutex forks[N];
    std::mutex io_mu;

    auto philosopher = [&](int id){
        for (int meal = 0; meal < 3; ++meal) {
            // Think.
            std::this_thread::sleep_for(std::chrono::milliseconds(50 + 30*id));
            // Eat: lock BOTH forks atomically (scoped_lock prevents deadlock).
            std::scoped_lock g(forks[id], forks[(id+1) % N]);
            {
                std::lock_guard io(io_mu);
                std::cout << "P" << id << " eating meal " << meal << "\n";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        std::lock_guard io(io_mu);
        std::cout << "P" << id << " done\n";
    };

    std::vector<std::thread> ts;
    for (int i = 0; i < N; ++i) ts.emplace_back(philosopher, i);
    for (auto& t : ts) t.join();
}
