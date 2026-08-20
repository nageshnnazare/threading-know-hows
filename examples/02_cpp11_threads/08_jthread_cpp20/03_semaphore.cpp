// 03_semaphore.cpp -- C++20 std::counting_semaphore / binary_semaphore.
//
//      std::counting_semaphore<MaxCount> sem(initial);
//
//          .acquire()              like P/sem_wait: block, then decrement
//          .release(n=1)           like V/sem_post: increment by n
//          .try_acquire()          non-blocking
//          .try_acquire_for(dur)   timed
//
//  std::binary_semaphore is just std::counting_semaphore<1>.
//
//  Use cases: token bucket, connection pool, signal-from-irq-to-thread.

#include <version>
#include <iostream>

#if __cpp_lib_semaphore >= 201907L
#include <semaphore>
#include <thread>
#include <vector>
#include <chrono>
using namespace std::chrono_literals;

std::counting_semaphore<3> pool(3);     // 3 connections allowed

void user(int id)
{
    pool.acquire();
    std::cout << "  user " << id << " in pool\n";
    std::this_thread::sleep_for(300ms);
    std::cout << "  user " << id << " out\n";
    pool.release();
}

int main()
{
    std::vector<std::thread> ts;
    for (int i = 0; i < 8; ++i) ts.emplace_back(user, i);
    for (auto& t : ts) t.join();
}
#else
int main(){ std::cout<<"need -std=c++20 for std::semaphore\n";}
#endif
