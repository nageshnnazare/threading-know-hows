// 04_compare_exchange.cpp -- CAS: the heart of lock-free programming.
//
//  Compare-And-Swap (CAS) atomically does:
//
//      if (atomic == expected) { atomic = desired; return true;  }
//      else                    { expected = atomic; return false; }
//
//  Two variants:
//
//      compare_exchange_weak    -- may fail spuriously even when equal
//                                  (useful inside a retry loop)
//      compare_exchange_strong  -- only fails when actually unequal
//                                  (useful when you don't loop)
//
//  Cost model: the WEAK form maps to a single CPU instruction on most
//  architectures (e.g. ARM's LL/SC); the STRONG form may emit an internal
//  loop on those architectures.
//
//  Canonical "atomically transform a value" pattern:
//
//      auto old = atomic.load();
//      while (!atomic.compare_exchange_weak(old, transform(old))) ;
//
//  ASCII (CAS retry under contention):
//
//      atomic = X
//      thread A: old=X, attempt X -> X+1   .. CAS succeeds
//      thread B: old=X, attempt X -> X+1   .. CAS fails (now atomic=X+1)
//                old=X+1, attempt X+1 -> X+2   .. retry succeeds

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

std::atomic<long> n{0};

// fetch_add equivalent built from CAS:
long my_add(std::atomic<long>& a, long delta)
{
    long old = a.load(std::memory_order_relaxed);
    while (!a.compare_exchange_weak(old, old + delta,
                                    std::memory_order_acq_rel,
                                    std::memory_order_relaxed))
        ;   // loop body intentionally empty: CAS reloads `old` for us
    return old;
}

// "Atomic max" -- not provided by std::atomic (yet).
void atomic_max(std::atomic<long>& a, long candidate)
{
    long current = a.load(std::memory_order_relaxed);
    while (candidate > current &&
           !a.compare_exchange_weak(current, candidate,
                                    std::memory_order_acq_rel,
                                    std::memory_order_relaxed))
        ;
}

int main()
{
    std::vector<std::thread> ts;
    for (int t = 0; t < 8; ++t)
        ts.emplace_back([]{ for (int i = 0; i < 100000; ++i) my_add(n, 1); });
    for (auto& t : ts) t.join();
    std::cout << "n=" << n.load() << " (expected 800000)\n";

    std::atomic<long> hi{0};
    ts.clear();
    for (int t = 0; t < 8; ++t)
        ts.emplace_back([t, &hi]{
            for (long i = 0; i < 1000; ++i)
                atomic_max(hi, t * 1000 + i);
        });
    for (auto& t : ts) t.join();
    std::cout << "hi=" << hi.load() << " (expected 7999)\n";
}
