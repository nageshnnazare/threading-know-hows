// 06_store_load_reorder.cpp -- Demonstrating store->load reordering.
//
//  Even on strong x86, ONE reordering is allowed: a thread's own store may
//  be observed BY OTHERS as if it happened AFTER a later load. (TSO.)
//
//      thread 1:        thread 2:
//      x.store(1)       y.store(1)
//      r1 = y.load()    r2 = x.load()
//
//  Outcome r1=0, r2=0 IS POSSIBLE with relaxed/even acquire+release loads,
//  because each core's store buffer drains independently.
//
//  Adding seq_cst stores (or a full fence between store and load) eliminates
//  this. We measure the rate of (0,0) outcomes for both modes.

#include <atomic>
#include <iostream>
#include <thread>

std::atomic<int> x{0}, y{0};
int r1, r2;

template <std::memory_order MO>
int run_once()
{
    x.store(0, std::memory_order_relaxed);
    y.store(0, std::memory_order_relaxed);

    std::thread t1([]{ x.store(1, MO); r1 = y.load(MO); });
    std::thread t2([]{ y.store(1, MO); r2 = x.load(MO); });
    t1.join(); t2.join();
    return (r1 == 0 && r2 == 0) ? 1 : 0;
}

int main()
{
    constexpr int RUNS = 5000;

    int relaxed_zeros = 0, seqcst_zeros = 0;
    for (int i = 0; i < RUNS; ++i) relaxed_zeros += run_once<std::memory_order_relaxed>();
    for (int i = 0; i < RUNS; ++i) seqcst_zeros  += run_once<std::memory_order_seq_cst>();

    std::cout << "relaxed (0,0) outcomes: " << relaxed_zeros << " / " << RUNS << "\n";
    std::cout << "seq_cst (0,0) outcomes: " << seqcst_zeros  << " / " << RUNS
              << " (must be 0)\n";
}
