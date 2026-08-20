# Threading API Cheat Sheet

Side-by-side mapping of the same synchronization concept across **pthreads (C)**,
**C++ standard threading**, and **OpenMP** (where applicable). Scannable tables;
see linked chapters for semantics and pitfalls.

---

## Thread create / join / detach

| Concept | pthreads (C) | C++ standard | OpenMP |
|---------|--------------|--------------|--------|
| Create | `pthread_create(&tid, attr, fn, arg)` | `std::thread t(fn, args...)` | `#pragma omp parallel` (team of threads) |
| Join (wait for exit) | `pthread_join(tid, &retval)` | `t.join()` | implicit at end of parallel region |
| Detach (fire-and-forget) | `pthread_detach(tid)` | `t.detach()` | N/A |
| Self | `pthread_self()` | `std::this_thread::get_id()` | `omp_get_thread_num()` |
| Exit | `pthread_exit(retval)` | return from thread function | `return` from task / implicit |
| Hardware threads hint | — | `std::thread::hardware_concurrency()` | `omp_get_max_threads()` |
| Stop / cancel | `pthread_cancel(tid)` (cooperative) | C++20: `std::jthread` + `stop_token` | `omp cancel parallel` (4.0+) |

Headers: `<pthread.h>` · `<thread>` · `<omp.h>`

→ [Part 1.1](../01-pthreads/01-thread-lifecycle.md) · [Part 2.1](../02-cpp-threads/01-std-thread.md) · [Part 6.1](../06-openmp/01-openmp-model.md)

---

## Mutex

| Concept | pthreads (C) | C++ standard | OpenMP |
|---------|--------------|--------------|--------|
| Basic lock | `pthread_mutex_t` · `lock/unlock/trylock` | `std::mutex` · `lock/unlock/try_lock` | `#pragma omp critical` (named optional) |
| RAII guard | manual | `std::lock_guard` · `std::unique_lock` | — |
| Recursive | `PTHREAD_MUTEX_RECURSIVE` | `std::recursive_mutex` | — |
| Timed | `pthread_mutex_timedlock` | `std::timed_mutex` | — |
| Multi-lock (deadlock-free) | manual ordering | `std::scoped_lock(a,b)` · `std::lock(a,b)` | — |
| Init static | `PTHREAD_MUTEX_INITIALIZER` | default-construct | — |

→ [Part 1.2](../01-pthreads/02-mutexes.md) · [Part 2.2](../02-cpp-threads/02-mutexes-and-locks.md)

---

## Condition variable

| Concept | pthreads (C) | C++ standard | OpenMP |
|---------|--------------|--------------|--------|
| Type | `pthread_cond_t` | `std::condition_variable` | — |
| Wait (must hold mutex) | `pthread_cond_wait(&cv, &m)` | `cv.wait(lock, pred)` | — |
| Timed wait | `pthread_cond_timedwait` | `wait_for` / `wait_until` | — |
| Wake one / all | `signal` / `broadcast` | `notify_one` / `notify_all` | — |
| Any lockable | — | `std::condition_variable_any` | — |
| Predicate loop | manual `while` | overload with `predicate` | — |

→ [Part 1.3](../01-pthreads/03-condition-variables.md) · [Part 2.3](../02-cpp-threads/03-condition-variables.md) · [Part 4.2](../04-patterns/02-producer-consumer.md)

---

## Read-write lock

| Concept | pthreads (C) | C++ standard | OpenMP |
|---------|--------------|--------------|--------|
| Type | `pthread_rwlock_t` | `std::shared_mutex` (C++17) | — |
| Shared (read) | `pthread_rwlock_rdlock` | `std::shared_lock` | — |
| Exclusive (write) | `pthread_rwlock_wrlock` | `std::unique_lock` | — |
| Unlock | `pthread_rwlock_unlock` | destructor / `unlock()` | — |

→ [Part 1.4](../01-pthreads/04-rwlocks-and-barriers.md) · [Part 4.3](../04-patterns/03-readers-writers.md)

---

## Semaphore

| Concept | pthreads (C) | C++ standard | OpenMP |
|---------|--------------|--------------|--------|
| Counting | `sem_t` · `sem_init/wait/post/destroy` | C++20: `std::counting_semaphore<N>` · `acquire/release` | — |
| Binary | `sem_init(&s,0,1)` | `std::binary_semaphore` | — |
| Header | `<semaphore.h>` | `<semaphore>` | — |

→ [Part 1.5](../01-pthreads/05-semaphores-and-spinlocks.md) · [Part 2.6](../02-cpp-threads/06-cpp20-concurrency.md)

---

## Barrier

| Concept | pthreads (C) | C++ standard | OpenMP |
|---------|--------------|--------------|--------|
| Type | `pthread_barrier_t` (GNU/BSD) | C++20: `std::barrier` | `#pragma omp barrier` |
| Init / wait | `barrier_init` · `barrier_wait` | `barrier.arrive_and_wait()` | implicit + explicit |
| One-shot latch | — | C++20: `std::latch` | — |

→ [Part 1.4](../01-pthreads/04-rwlocks-and-barriers.md) · [Part 2.6](../02-cpp-threads/06-cpp20-concurrency.md) · [Part 6.3](../06-openmp/03-synchronization-and-reductions.md)

---

## Atomics

| Concept | C11 `<stdatomic.h>` | C++ `<atomic>` | OpenMP |
|---------|---------------------|----------------|--------|
| Type | `atomic_int`, `_Atomic(T)` | `std::atomic<T>` | `#pragma omp atomic` |
| Load / store | `atomic_load/store` | `.load(MO)` · `.store(v,MO)` | read / write / update |
| RMW | `atomic_fetch_add` | `.fetch_add(v,MO)` | `atomic capture` |
| CAS | `atomic_compare_exchange` | `.compare_exchange_weak/strong` | — |
| Flag | `atomic_flag` | `std::atomic_flag` | — |
| Fence | `atomic_thread_fence` | `std::atomic_thread_fence(MO)` | — |
| Memory orders | `memory_order_*` | `std::memory_order_*` | flush (implementation-defined) |

→ [Part 3.1–3.5](../03-atomics-and-memory-model/01-atomics-basics.md)

---

## Thread-local storage (TLS)

| Concept | pthreads (C) | C++ standard | OpenMP |
|---------|--------------|--------------|--------|
| Key-based | `pthread_key_create` · `setspecific/getspecific` | — | — |
| Language TLS | `_Thread_local` (C11) | `thread_local` | — |
| Per-thread private | — | — | `#pragma omp threadprivate(x)` |

→ [Part 1.6](../01-pthreads/06-tls-and-cancellation.md) · [Part 2.5](../02-cpp-threads/05-call-once-and-thread-local.md)

---

## One-time initialization

| Concept | pthreads (C) | C++ standard | OpenMP |
|---------|--------------|--------------|--------|
| Once flag | `pthread_once_t` · `pthread_once` | `std::once_flag` · `std::call_once` | — |
| Magic static | — | `static T obj;` in function (C++11 thread-safe) | — |

→ [Part 2.5](../02-cpp-threads/05-call-once-and-thread-local.md) · [Part 4.7](../04-patterns/07-double-checked-locking.md)

---

## Futures / async tasks

| Concept | pthreads (C) | C++ standard | OpenMP |
|---------|--------------|--------------|--------|
| Async call | roll your own + join | `std::async(launch::async, fn, args...)` | `#pragma omp task` |
| Result channel | — | `std::future` · `std::promise` | task `depend` / reduction |
| Packaged work | — | `std::packaged_task` | — |
| Thread pool submit | manual queue | (no std pool; see Part 4.1) | taskloop |

→ [Part 2.4](../02-cpp-threads/04-futures-and-async.md) · [Part 4.1](../04-patterns/01-thread-pool.md) · [Part 6.4](../06-openmp/04-tasks.md)

---

## Spinlock

| Concept | pthreads (C) | C++ standard | OpenMP |
|---------|--------------|--------------|--------|
| Type | `pthread_spinlock_t` | `std::atomic_flag` test-and-set loop | — |
| Use when | hold time ≪ context switch | same | — |

→ [Part 1.5](../01-pthreads/05-semaphores-and-spinlocks.md)

---

## Compile / link appendix

| Language / API | Typical compile line |
|----------------|----------------------|
| C + pthreads | `gcc -Wall -pthread file.c -o file` |
| C++11 threads | `g++ -std=c++17 -pthread file.cpp -o file` |
| C++20 (`jthread`, `latch`, `barrier`, `semaphore`) | `g++ -std=c++20 -pthread file.cpp -o file` |
| OpenMP (GCC/Clang) | `gcc -fopenmp file.c -o file` · `g++ -std=c++17 -fopenmp file.cpp -o file` |
| ThreadSanitizer | `g++ -std=c++17 -O1 -g -fsanitize=thread -pthread file.cpp -o file` |

Link note: on Linux, `-pthread` sets both compile and link flags. macOS Clang often accepts `-pthread`; some setups need explicit `-lpthread` for pure C.

---

## Rules of thumb

1. **Shared mutable + concurrent ⇒ synchronize** — mutex, atomic, or documented `memory_order`; else data race → UB ([Part 0.4](../00-foundations/04-the-shared-state-problem.md)).
2. **Prefer the simplest primitive that works**: atomic word → mutex → rwlock → lock-free ([Part 3.2](../03-atomics-and-memory-model/02-lock-free-and-cas.md)).
3. **Always `wait(lock, predicate)`** on condition variables ([Part 5.6](../05-pitfalls/06-lost-wakeups.md)).
4. **Hold locks for the shortest critical section** — no I/O, no blocking calls while locked.
5. **RAII locks only** — `lock_guard`, `unique_lock`, `scoped_lock`; never manual unlock paths ([Part 2.2](../02-cpp-threads/02-mutexes-and-locks.md)).
6. **Consistent lock order** or `scoped_lock` / `std::lock` for multiple mutexes ([Part 5.2](../05-pitfalls/02-deadlock.md)).
7. **Pad to cache lines** when adjacent variables are written by different threads ([Part 5.4](../05-pitfalls/04-false-sharing.md)).
8. **Acquire/release pair** for hand-off synchronization; default to `seq_cst` until you measure ([Part 3.4](../03-atomics-and-memory-model/04-memory-orders.md)).
9. **Size pools** near `hardware_concurrency()` for CPU-bound work ([Part 4.1](../04-patterns/01-thread-pool.md)).
10. **Run under ThreadSanitizer** during development — races that "work on my machine" are still UB.

---

Back to [README](../README.md)
