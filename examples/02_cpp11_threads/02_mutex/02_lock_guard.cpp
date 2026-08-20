// 02_lock_guard.cpp -- The smallest, fastest RAII lock helper.
//
//  std::lock_guard<Mutex> locks the mutex in its constructor and unlocks
//  it in its destructor. That's it -- no .unlock(), no early release.
//  Perfect for single-mutex critical sections.
//
//      {
//          std::lock_guard<std::mutex> g(m);   // m.lock() now
//          // ... critical section ...
//      }                                       // m.unlock() here, even on throw
//
//  ASCII flow:
//
//                            ctor
//                           /    \
//      enter scope ----->  /      \-----> m.lock()
//      ...critical...
//      exit scope (incl. throw) ------> m.unlock()  <-- via dtor
//
//  Compared to manual lock/unlock:
//
//      Manual                   lock_guard
//      ------------             ----------
//      m.lock();                {
//      try {                       std::lock_guard g(m);
//        ...                       ...
//        m.unlock();             }   // automatic
//      } catch (...) {
//        m.unlock();
//        throw;
//      }
//
//  C++17 added template argument deduction so you can write:
//      std::lock_guard g(m);              // type deduced

#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

class Counter {
    mutable std::mutex m_;
    long              v_ = 0;
public:
    void inc()  { std::lock_guard<std::mutex> g(m_); ++v_; }
    long get() const {
        std::lock_guard<std::mutex> g(m_);
        return v_;
    }
};

int main()
{
    Counter c;
    constexpr int N = 8, ITERS = 100000;
    std::vector<std::thread> ts;
    for (int i = 0; i < N; ++i)
        ts.emplace_back([&]{ for (int k=0;k<ITERS;++k) c.inc(); });
    for (auto& t : ts) t.join();
    std::cout << "value = " << c.get()
              << " (expected " << N*ITERS << ")\n";
}
