# Modern C++ Threads (C++11 / C++14 / C++17 / C++20)

Standard C++ added threading in C++11. Each newer revision sharpened it. This
section covers the entire ecosystem with diagrams.

```
   <thread>           <mutex>             <condition_variable>     <atomic>
   --------           -------             --------------------     --------
   std::thread        std::mutex          std::condition_variable  std::atomic<T>
   std::jthread (20)  std::recursive_     std::condition_variable_ std::atomic_flag
                       mutex               any
                      std::timed_mutex                             std::memory_order
                      std::shared_mutex
                      std::lock_guard
                      std::unique_lock
                      std::scoped_lock(17)

   <future>           <chrono>            <semaphore> (20)         <barrier> (20)
   --------           -------             ----------------         ----------------
   std::async         milliseconds        std::counting_semaphore  std::barrier
   std::future        seconds             std::binary_semaphore    std::latch
   std::promise
   std::packaged_task
   std::shared_future
```

## Why prefer C++11 std:: over pthreads?

| Aspect            | pthreads        | std::thread / std::* |
|-------------------|-----------------|----------------------|
| Type safety       | `void*` casts   | Templates / variadic |
| RAII              | manual          | Built-in (lock_guard, jthread) |
| Cancellation      | `pthread_cancel` (dangerous) | `std::stop_token` (C++20, cooperative) |
| Atomics           | not in pthread  | `std::atomic<T>` with memory orders |
| Cross-platform    | POSIX only      | works on Windows, Linux, macOS, embedded |

## Structure

1. `01_basics/`  - launching, joining, detaching, ids, hardware concurrency
2. `02_mutex/`   - mutex, recursive, timed, shared, lock_guard, unique_lock, scoped_lock
3. `03_condition_variables/` - cv, cv_any, predicates, broadcast
4. `04_atomics/` - atomic types, memory orders, CAS, lock-free stack
5. `05_futures/` - async, promise, packaged_task, shared_future
6. `06_call_once/` - thread-safe one-shot initialization
7. `07_thread_local/` - per-thread variables
8. `08_jthread_cpp20/` - std::jthread, std::stop_token, std::barrier, latch, semaphore

## Build

```
make            # build all C++ examples
make run        # run them sequentially
make clean
```
