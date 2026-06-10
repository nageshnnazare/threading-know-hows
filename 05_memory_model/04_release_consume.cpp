// 04_release_consume.cpp -- The (de-facto deprecated) consume order.
//
//  memory_order_consume was meant to be a CHEAPER acquire that only orders
//  reads "data-dependent" on the loaded value. In practice no compiler
//  implements it as designed; they all promote it to acquire.
//
//      Treat consume as a synonym for acquire for now and avoid using it.
//
//  This file is included for completeness so you recognize the order if you
//  see it in legacy code.
//
//  Picture (intent that never materialized):
//
//      pointer published with release ----.
//      reader does:                       v
//          p = atomic.load(consume)       ordering only for reads through p,
//          x = p->field                   not unrelated data.

#include <atomic>
#include <iostream>
#include <thread>

struct Cfg { int a; int b; };
std::atomic<Cfg*> g{nullptr};

int main()
{
    std::thread w([]{
        Cfg* c = new Cfg{10, 20};
        g.store(c, std::memory_order_release);
    });

    std::thread r([]{
        Cfg* c = nullptr;
        while (!(c = g.load(std::memory_order_consume))) ;
        std::cout << "consume reader: a=" << c->a << " b=" << c->b << "\n";
    });
    w.join(); r.join();
    std::cout << "(in current compilers consume == acquire)\n";
}
