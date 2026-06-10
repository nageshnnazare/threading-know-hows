# C/C++ Multi-Threading & Parallel Programming - The Complete Guide

A comprehensive, hands-on tutorial that walks you through every important concept in
multi-threaded and parallel programming in C and C++. Each example is small,
self-contained, and accompanied by ASCII diagrams that visualize what is happening
in memory and across threads.

```
                +-------------------------------------------------+
                |   ONE STOP GUIDE: PTHREADS + C++11 + ATOMICS    |
                +-------------------------------------------------+
                            |
        +-------------------+-------------------+
        |                   |                   |
   +----v-----+        +----v-----+        +----v------+
   | pthreads |        |  C++11+  |        |  OpenMP   |
   |  (POSIX) |        |  Threads |        | (compiler |
   |          |        |  std::*  |        |   pragmas)|
   +----+-----+        +----+-----+        +----+------+
        |                   |                   |
        +---------+---------+---------+---------+
                  |                   |
            +-----v------+      +-----v-------+
            |  Patterns  |      |  Pitfalls   |
            | (pool,P/C, |      | (race, dead |
            |  pipeline) |      |  lock, ABA) |
            +------------+      +-------------+
```

## Table of Contents

| #  | Section                                          | Description |
|----|--------------------------------------------------|-------------|
| 1  | [pthreads](01_pthreads/)                         | The POSIX threading API in pure C |
| 2  | [C++11 threads](02_cpp11_threads/)               | Modern C++ threading - `std::thread`, `std::mutex`, `std::atomic`, `std::future`, etc. |
| 3  | [Concurrency patterns](03_patterns/)             | Thread pool, producer-consumer, readers-writers, dining philosophers, pipeline, work-stealing |
| 4  | [Pitfalls](04_pitfalls/)                         | Race conditions, deadlocks, livelock, starvation, false sharing, ABA |
| 5  | [Memory model](05_memory_model/)                 | Sequential consistency, acquire/release, relaxed, fences, hardware reordering |
| 6  | [OpenMP](06_openmp/)                             | Compiler-driven parallelism for loops, sections, reductions |
| C  | [CHEATSHEET.md](CHEATSHEET.md)                   | One-page printable reference of all APIs and rules of thumb |

## Suggested Learning Path

```
   Beginner                Intermediate                Advanced
   --------                ------------                --------
   01_pthreads/01_basics
        |
        v
   01_pthreads/02_mutex   ----> 02_cpp11_threads/02_mutex
        |                              |
        v                              v
   01_pthreads/03_cv      ----> 02_cpp11_threads/03_cv
        |                              |
        v                              v
   04_pitfalls (race,            02_cpp11_threads/04_atomics
   deadlock first)                    |
        |                              v
        v                       05_memory_model
   03_patterns/01_thread_pool         |
        |                              v
        v                       03_patterns/07_work_stealing
   06_openmp                           |
                                       v
                                04_pitfalls/05_false_sharing
                                04_pitfalls/06_aba_problem
```

## Building & Running

Each subdirectory has its own `Makefile`. From any sub-folder:

```bash
make           # build all examples in that folder
make run       # build and run them sequentially
make clean     # remove binaries
```

Or to build everything:

```bash
make -C 01_pthreads
make -C 02_cpp11_threads
make -C 03_patterns
make -C 04_pitfalls
make -C 05_memory_model
make -C 06_openmp
```

### Compilers and Flags

| Component | Compiler | Required flags |
|-----------|----------|----------------|
| pthreads  | gcc/clang | `-pthread` |
| C++11+    | g++/clang++ | `-std=c++17 -pthread` |
| C++20 jthread | g++ >= 10, clang++ >= 12 | `-std=c++20 -pthread` |
| OpenMP    | gcc | `-fopenmp` |
| Sanitizers (highly recommended while learning) | gcc/clang | `-fsanitize=thread` or `-fsanitize=address` |

## Mental Model

Before diving in, remember the four "What can go wrong?" questions every threaded
program must answer:

```
   +----------------------+       +-------------------------+
   | 1. Atomicity         |  ===> | Can another thread see  |
   |                      |       | a half-finished update? |
   +----------------------+       +-------------------------+

   +----------------------+       +-------------------------+
   | 2. Visibility        |  ===> | When I write, when does |
   |                      |       | the other thread SEE it?|
   +----------------------+       +-------------------------+

   +----------------------+       +-------------------------+
   | 3. Ordering          |  ===> | In what order do other  |
   |                      |       | threads see my writes?  |
   +----------------------+       +-------------------------+

   +----------------------+       +-------------------------+
   | 4. Liveness          |  ===> | Is the program making   |
   |                      |       | progress (no deadlock)? |
   +----------------------+       +-------------------------+
```

Every primitive in this guide is a tool to address one or more of those four
questions. Keep them in mind while reading.

## Concepts At-a-Glance

```
   PROCESS
   +---------------------------------------------------------+
   |  Address space (heap, globals, code)  -- SHARED         |
   |                                                         |
   |  +-----------+    +-----------+    +-----------+        |
   |  | Thread 1  |    | Thread 2  |    | Thread 3  |        |
   |  |  stack    |    |  stack    |    |  stack    |        |
   |  |  regs/PC  |    |  regs/PC  |    |  regs/PC  |        |
   |  |  TLS      |    |  TLS      |    |  TLS      |        |
   |  +-----------+    +-----------+    +-----------+        |
   |                                                         |
   |   ^-- private to each thread                            |
   +---------------------------------------------------------+

   Threads share : heap, globals, file descriptors, code segment
   Threads have  : own stack, registers, program counter, TLS
```

Happy hacking, and remember: **if your code is racy, it's wrong - even if it
"works"**. Use sanitizers, write small examples, prove correctness with
diagrams before relying on intuition.
