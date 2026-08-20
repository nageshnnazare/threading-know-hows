// 02_latch_barrier.cpp -- C++20 std::latch and std::barrier.
//
//   std::latch n        -- single-shot countdown; threads count_down(),
//                          waiters block on wait() until count reaches 0.
//
//   std::barrier n      -- reusable: N threads call .arrive_and_wait() each
//                          phase; everyone is released when N arrive.
//
//  When to use which:
//
//      one-shot "wait until N events happened"  ->  latch
//      multi-phase rendezvous                   ->  barrier
//
//  Picture (latch n=3):
//
//      counter starts at 3
//      thread A count_down -> 2
//      waiter   wait()       (blocked)
//      thread B count_down -> 1
//      thread C count_down -> 0  -> ALL waiters release

#include <version>
#include <iostream>

#if __cpp_lib_latch >= 201907L && __cpp_lib_barrier >= 201907L
#include <latch>
#include <barrier>
#include <thread>
#include <chrono>
#include <vector>
using namespace std::chrono_literals;

int main()
{
    /* ---------- latch ---------- */
    std::latch ready(3);
    std::vector<std::thread> ts;
    for (int i = 0; i < 3; ++i)
        ts.emplace_back([&, i]{
            std::this_thread::sleep_for((i + 1) * 100ms);
            std::cout << "  worker " << i << " ready\n";
            ready.count_down();
        });

    ready.wait();
    std::cout << "[main] all ready\n";
    for (auto& t : ts) t.join();

    /* ---------- barrier (reusable) ---------- */
    std::barrier sync_point(3, []() noexcept {
        std::cout << "  --- phase complete ---\n";
    });

    ts.clear();
    for (int i = 0; i < 3; ++i)
        ts.emplace_back([&, i]{
            for (int phase = 1; phase <= 3; ++phase) {
                std::this_thread::sleep_for((i + 1) * 80ms);
                std::cout << "  T" << i << " phase " << phase << "\n";
                sync_point.arrive_and_wait();
            }
        });
    for (auto& t : ts) t.join();
}
#else
int main(){ std::cout<<"need -std=c++20 and recent libc++ for latch/barrier\n";}
#endif
