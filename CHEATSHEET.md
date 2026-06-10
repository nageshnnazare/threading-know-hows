# Threading Cheat Sheet

A printable, single-page reference. Pair with the code in this repo.

## 1. Decision tree: which primitive?

```
  Need to PROTECT shared data?
       |
       v
  Just one variable, fits in a word, only ++/--/load/store?
       |
       +--YES--> std::atomic<T>            (lockless, fastest)
       |
       NO
       |
       v
  Read-mostly, expensive read?
       |
       +--YES--> std::shared_mutex (writer rare) or
                 atomic snapshot pointer (RCU style)
       |
       NO
       |
       v
  std::mutex + std::lock_guard       (default, simplest, correct)


  Need to WAIT for a CONDITION?
       |
       v
  std::condition_variable + predicate-loop wait/notify

  Need to RUN a callable in parallel and get a result?
       |
       v
  std::async(std::launch::async, fn, args)   then  fut.get()

  Need a one-shot init?
       |
       v
  Function-local static  OR  std::call_once

  Need to bound concurrent access to N resources?
       |
       v
  std::counting_semaphore<N> (C++20)   or   POSIX sem_t

  Need to wait for N events?      -> std::latch  (one-shot)
  Need a multi-phase rendezvous?  -> std::barrier (reusable)

  Need many short tasks?          -> Thread pool

  Do tasks have producer/consumer
  shape with bounded buffer?      -> Bounded thread-safe queue +
                                     condition variables
```

## 2. pthread quick reference

```c
pthread_t          tid;
pthread_create(&tid, NULL, fn, arg);
pthread_join(tid, &retval);
pthread_detach(tid);
pthread_self();
pthread_equal(t1, t2);
pthread_exit(retval);
pthread_cancel(tid);                     // cooperative
pthread_setcancelstate(STATE, NULL);
pthread_setcanceltype(TYPE, NULL);

// Mutex
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_lock/unlock/trylock/timedlock(&m, ...);
// Recursive: PTHREAD_MUTEX_RECURSIVE via pthread_mutexattr_settype

// Condition variable
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
pthread_cond_wait(&cv, &m);              // mutex held; releases+sleeps
pthread_cond_timedwait(&cv, &m, &abs);
pthread_cond_signal/broadcast(&cv);

// rwlock
pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_rdlock/wrlock/unlock(&rw);

// barrier (Linux)
pthread_barrier_t b;
pthread_barrier_init(&b, NULL, N);
pthread_barrier_wait(&b);
pthread_barrier_destroy(&b);

// Semaphore (POSIX)
sem_t s; sem_init(&s, 0, N); sem_wait(&s); sem_post(&s); sem_destroy(&s);

// Spinlock
pthread_spinlock_t sl;
pthread_spin_init(&sl, PTHREAD_PROCESS_PRIVATE);
pthread_spin_lock/unlock(&sl);

// TLS
pthread_key_t k;
pthread_key_create(&k, dtor);
pthread_setspecific(k, ptr);
void* p = pthread_getspecific(k);
```

## 3. C++ standard quick reference

```cpp
// thread
#include <thread>
std::thread t(fn, args...);   t.join();   t.detach();
std::this_thread::get_id() / sleep_for / yield;
std::thread::hardware_concurrency();

// jthread (C++20)
#include <thread>          // std::jthread
std::jthread jt([](std::stop_token st){ while (!st.stop_requested()) {} });
jt.request_stop();         // dtor auto-joins

// Mutex family
#include <mutex>           <shared_mutex>
std::mutex                  // basic
std::recursive_mutex        // same thread re-locks
std::timed_mutex            // try_lock_for/_until
std::shared_mutex (C++17)   // many readers / one writer
std::lock_guard             // RAII, one mutex
std::unique_lock            // movable, deferable, cv-compatible
std::scoped_lock (C++17)    // RAII, multiple mutexes, deadlock-free
std::shared_lock (C++14)    // RAII reader of shared_mutex

// Condition variable
#include <condition_variable>
std::condition_variable      cv;          // needs unique_lock<mutex>
std::condition_variable_any  cva;         // any Lockable
cv.wait(lock, predicate);   cv.notify_one() / notify_all();
cv.wait_for / wait_until;

// Atomic
#include <atomic>
std::atomic<T> a;
a.load(MO);  a.store(v, MO);  a.exchange(v, MO);
a.compare_exchange_weak/strong(expected, desired, succ_MO, fail_MO);
a.fetch_add/sub/and/or/xor(v, MO);
std::atomic_flag f = ATOMIC_FLAG_INIT;
f.test_and_set(MO);  f.clear(MO);
std::atomic_thread_fence(MO);

// Memory orders
std::memory_order_relaxed      // atomicity only
std::memory_order_acquire      // pairs with release
std::memory_order_release
std::memory_order_acq_rel      // RMW
std::memory_order_seq_cst      // default; total order

// Futures
#include <future>
auto f = std::async(std::launch::async, fn, args...);
f.get(); f.wait(); f.wait_for(dur);

std::promise<T> p; auto fut = p.get_future(); p.set_value(v);
std::packaged_task<R(Args...)> t(fn); auto fut = t.get_future(); t(args...);
std::shared_future<T> sf = fut.share();      // copyable, multi-reader

// One-time init
#include <mutex>
std::once_flag once;
std::call_once(once, init_fn, args...);

// Per-thread
thread_local int cache;

// C++20 sync
#include <latch>      std::latch L(n); L.count_down(); L.wait();
#include <barrier>    std::barrier B(n[, completion]); B.arrive_and_wait();
#include <semaphore>  std::counting_semaphore<MAX> s(init); s.acquire(); s.release();
#include <stop_token> std::stop_source / std::stop_token / std::stop_callback
```

## 4. Common patterns at-a-glance

```cpp
// === Counter ===
std::atomic<long> n{0};      ++n;            // good

// === Lazy singleton ===
Foo& instance() { static Foo f; return f; }   // C++11+ thread-safe

// === Producer/consumer ===
std::unique_lock g(m);
cv.wait(g, [&]{ return ready; });
... use ...
g.unlock();    cv.notify_one();

// === Multi-mutex (no deadlock) ===
std::scoped_lock g(a.m, b.m);

// === Thread-safe init ===
std::call_once(once, []{ init(); });

// === Stop a worker (modern) ===
std::jthread w([](std::stop_token st){
    while (!st.stop_requested()) tick();
});
```

## 5. Memory order picture

```
   Strongest, slowest                                    Weakest, fastest
   ------------------                                    ----------------
       seq_cst   >   acq_rel   >  acquire/release  >  consume(=acq)  >  relaxed

   "Total order"   "RMW pair"      "1:1 sync edge"                    "atomic only"
   visible to all                  for paired ops
```

The bread-and-butter pattern is **acquire/release**:

```
   producer:  ... data writes ...
              flag.store(true, std::memory_order_release);
                                      |
                                      v synchronizes-with
   consumer:  while (!flag.load(std::memory_order_acquire)) {}
              ... read the data the producer wrote ...
```

## 6. Sanitizers - your best friend while learning

```bash
g++ -std=c++17 -O1 -g -fsanitize=thread   prog.cpp -o prog -pthread   # data races
g++ -std=c++17 -O1 -g -fsanitize=address  prog.cpp -o prog -pthread   # mem bugs
g++ -std=c++17 -O1 -g -fsanitize=undefined prog.cpp -o prog -pthread  # UB
```

Run normally; sanitizer reports diagnostics on stdout/stderr at runtime.

## 7. Rules of thumb

1. **If your code has shared mutable state and isn't synchronized, it's broken** -- even if it "works on my machine".
2. **Prefer the simplest primitive that works**: atomic > mutex > rwlock > lock-free.
3. **Always use the predicate form of `cv.wait`** to be safe against spurious wakeups and missed notifications.
4. **Lock as little as possible**: small critical sections, no I/O while locked.
5. **Always release locks via RAII** (`lock_guard`/`scoped_lock`/`unique_lock`). Never hand-managed.
6. **Acquire locks in a consistent order** (or use `scoped_lock`/`std::lock`).
7. **Pad to cache lines** when threads write to nearby variables.
8. **Don't reinvent lock-free** unless you understand the memory model AND have a plan for memory reclamation (hazard pointers, RCU, etc.).
9. **Profile before optimizing** -- mutex-protected code is often fast enough; the perf wins of lock-free can vanish in real workloads.
10. **Test under sanitizers**, not just optimistically. ThreadSanitizer is gold.
