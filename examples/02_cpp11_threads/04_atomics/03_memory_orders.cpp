// 03_memory_orders.cpp -- The MEMORY MODEL.
//
//  Modern CPUs and compilers reorder loads/stores. Memory orders tell the
//  HARDWARE/COMPILER which reorderings are forbidden so concurrent code
//  can rely on certain happens-before relationships.
//
//  Six memory_order values (C++11):
//
//      memory_order_relaxed       -- no ordering, only atomicity
//      memory_order_consume       -- (deprecated; treat as acquire)
//      memory_order_acquire       -- pair this with release; reads after
//                                    me see the writes the releaser made
//      memory_order_release       -- pair this with acquire; writes I made
//                                    are visible to the next acquirer
//      memory_order_acq_rel       -- both acquire AND release (RMW ops)
//      memory_order_seq_cst       -- (default) total order across ALL threads,
//                                    "everyone agrees on a single timeline"
//
//  Strength chart (expensive at top, free at bottom on x86):
//
//        seq_cst     <-- DEFAULT, simplest, slowest in some cases
//          |
//        acq_rel
//        /    \
//      acq    rel
//        \    /
//        relaxed     <-- fastest, almost no guarantees
//
//  THE ACQUIRE-RELEASE PATTERN (the workhorse):
//
//      shared data ----+---- store with RELEASE  --->  happens-before
//                                                       chain
//                                                        |
//      consume (load with ACQUIRE) <-- on another thread,  v
//      now sees the data the releaser wrote
//
//  Picture:
//
//      Thread A                       Thread B
//      --------                       --------
//      data = compute();
//      flag.store(1, RELEASE)  ----.
//                                   `--> flag.load(ACQUIRE) == 1
//                                        use(data);   <-- guaranteed visible
//
//  Without proper ordering, B might see flag==1 BEFORE seeing data --
//  classic bug. We demonstrate this distinction below.

#include <atomic>
#include <iostream>
#include <thread>
#include <cassert>

int                 data;
std::atomic<bool>   flag{false};

void producer()
{
    data = 42;                                        // (1) plain write
    flag.store(true, std::memory_order_release);      // (2) release
    // Reads/writes BEFORE (2) cannot be reordered to AFTER it.
}

void consumer()
{
    while (!flag.load(std::memory_order_acquire))     // (3) acquire
        ;
    // Reads/writes AFTER (3) cannot be reordered to BEFORE it.
    // Therefore (1) is visible here.
    assert(data == 42);
    std::cout << "consumer saw data=" << data << "\n";
}

int main()
{
    std::thread c(consumer), p(producer);
    p.join(); c.join();
}
