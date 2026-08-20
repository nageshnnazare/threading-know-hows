// 02_acquire_release.cpp -- The workhorse pattern.
//
//  Producer:                     Consumer:
//      ... write data ...            wait for flag (acquire)
//      flag.store(1, release)        ... read data ...
//
//  Establishes a synchronizes-with edge:
//
//      producer's release  --HAPPENS BEFORE-->  consumer's acquire
//
//  Anything the producer wrote BEFORE the release is visible AFTER the
//  matching acquire on the consumer.
//
//  ASCII timeline:
//
//      producer       :  data=...   release(flag=1)  ........
//                                      \
//                                       \ synchronizes-with
//                                        \
//      consumer       :  acquire(flag==1) -> reads data already finalized
//
//  Cheaper than seq_cst because no global ordering is required, only this
//  specific paired ordering.

#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>

int               data;
std::atomic<bool> ready{false};

void producer() {
    data = 1234;                                   // (1)
    ready.store(true, std::memory_order_release);  // (2): publishes (1)
}
void consumer() {
    while (!ready.load(std::memory_order_acquire)) // (3): synchronizes with (2)
        ;
    assert(data == 1234);                          // safe: (1) visible
}
int main() {
    std::thread c(consumer), p(producer);
    p.join(); c.join();
    std::cout << "data=" << data << "\n";
}
