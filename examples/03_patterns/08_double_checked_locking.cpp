// 08_double_checked_locking.cpp -- Lazy initialization, the right way.
//
//  Classic broken DCL (pre-C++11):
//
//      static Singleton* p = nullptr;
//      Singleton* get() {
//          if (!p) {                       <-- racy read
//              lock();
//              if (!p) p = new Singleton;  <-- racy: store before construction visible
//              unlock();
//          }
//          return p;
//      }
//
//  The bug: another thread can read `p` non-null but see Singleton's
//  members not yet initialized due to instruction reordering.
//
//  CORRECT alternatives:
//
//   1. C++11 Magic Statics (preferred):
//
//          Singleton& get() { static Singleton s; return s; } // thread-safe by std
//
//   2. std::call_once:
//
//          std::call_once(once, []{ p = new Singleton; });
//
//   3. std::atomic<Singleton*> with acquire/release if you really want DCL:
//
//          std::atomic<Singleton*> p{nullptr};
//          Singleton* get() {
//              auto s = p.load(memory_order_acquire);     // (1)
//              if (!s) {
//                  std::lock_guard g(m);
//                  s = p.load(memory_order_relaxed);
//                  if (!s) {
//                      s = new Singleton;
//                      p.store(s, memory_order_release);  // (2)
//                  }
//              }
//              return s;
//          }
//
//      Pairing (2) release with (1) acquire ensures readers that see p
//      non-null also see the FULLY constructed Singleton.
//
//  This file demonstrates ALL three approaches.

#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

struct Singleton {
    int    a = 1;
    double b = 2.5;
    Singleton() {
        // Imagine non-trivial init here.
    }
};

/* --- (1) magic statics --- */
Singleton& get_meyers() {
    static Singleton s;            // standard guarantees thread-safe init
    return s;
}

/* --- (2) call_once --- */
static std::once_flag g_once;
static Singleton*     g_call_once_ptr = nullptr;
Singleton* get_call_once() {
    std::call_once(g_once, []{ g_call_once_ptr = new Singleton(); });
    return g_call_once_ptr;
}

/* --- (3) atomic DCL --- */
static std::atomic<Singleton*> g_atomic{nullptr};
static std::mutex              g_atomic_m;
Singleton* get_atomic_dcl() {
    Singleton* s = g_atomic.load(std::memory_order_acquire);
    if (!s) {
        std::lock_guard g(g_atomic_m);
        s = g_atomic.load(std::memory_order_relaxed);
        if (!s) {
            s = new Singleton();
            g_atomic.store(s, std::memory_order_release);
        }
    }
    return s;
}

int main()
{
    std::vector<std::thread> ts;
    for (int i = 0; i < 8; ++i)
        ts.emplace_back([]{
            volatile auto* a = &get_meyers();
            volatile auto* b = get_call_once();
            volatile auto* c = get_atomic_dcl();
            (void)a; (void)b; (void)c;
        });
    for (auto& t : ts) t.join();
    std::cout << "all three methods initialized OK\n";
}
