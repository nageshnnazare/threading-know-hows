// 01_seq_cst.cpp -- memory_order_seq_cst (the default).
//
//  All seq_cst operations appear in a single global total order that all
//  threads agree on. This is the most intuitive but also strongest (and
//  often most expensive) mode.
//
//  Property: in DEKKER's algorithm the following can NEVER occur in seq_cst
//  but CAN happen in weaker models:
//
//      thread 1:  x.store(1);                t1 reads y == 0
//      thread 2:  y.store(1);                t2 reads x == 0
//
//  Because if both stores happened "before" both loads in the global total
//  order, at least one of the loads must see the corresponding store.
//
//  This file runs Dekker's pattern and asserts the impossible-with-seq_cst
//  outcome doesn't occur.

#include <atomic>
#include <iostream>
#include <thread>

std::atomic<int> x{0}, y{0};
int rx, ry;

int main()
{
    constexpr int RUNS = 1000;
    int seqcst_violations = 0;

    for (int i = 0; i < RUNS; ++i) {
        x.store(0);
        y.store(0);

        std::thread t1([]{
            x.store(1, std::memory_order_seq_cst);
            ry = y.load(std::memory_order_seq_cst);
        });
        std::thread t2([]{
            y.store(1, std::memory_order_seq_cst);
            rx = x.load(std::memory_order_seq_cst);
        });
        t1.join(); t2.join();

        if (rx == 0 && ry == 0) ++seqcst_violations;
    }
    std::cout << "Under seq_cst, both reads being 0 happened "
              << seqcst_violations << " times in " << RUNS
              << " runs (must be 0).\n";
}
