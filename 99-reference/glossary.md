# Glossary

Alphabetized definitions (2–4 sentences each). Terms link to the part/chapter
where the mechanism is developed in depth.

---

**ABA problem** — In lock-free algorithms using compare-and-swap, a location's value can change from A → B → A while a thread was paused; CAS succeeds even though the structure changed underneath. Breaks naive lock-free stacks and lists. Mitigations: tagged pointers, hazard pointers, epoch-based reclamation. → [Part 5.5](../05-pitfalls/05-aba-problem.md)

**acquire / release** — Memory orders that form a **synchronizes-with** edge: a **release** store on an atomic makes prior writes visible to threads that later **acquire**-load that atomic. The bread-and-butter hand-off pattern for publishing data between threads. → [Part 3.4](../03-atomics-and-memory-model/04-memory-orders.md)

**Amdahl's law** — Speedup from parallelizing a fraction *p* of a program is capped at 1 / ((1−p) + p/N) for N processors; the serial fraction dominates at scale. Explains why optimizing the bottleneck stage matters in pipelines and why not every loop parallelizes well. → [Part 0.2](../00-foundations/02-concurrency-vs-parallelism.md)

**atomic** — An operation (or `std::atomic`/`_Atomic` object) that executes as a single indivisible step with respect to other threads, without a mutex. Provides atomicity and, with memory orders, visibility guarantees. Too small for multi-field invariants without careful design. → [Part 3.1](../03-atomics-and-memory-model/01-atomics-basics.md)

**barrier** — A synchronization point where N threads must all arrive before any proceeds; used for phased algorithms ("generation 1 done, start generation 2"). Distinct from a memory fence. pthread `barrier`, C++20 `std::barrier`, OpenMP `#pragma omp barrier`. → [Part 1.4](../01-pthreads/04-rwlocks-and-barriers.md) · [Part 2.6](../02-cpp-threads/06-cpp20-concurrency.md)

**busy-wait** — Spinning in a loop checking a condition instead of blocking in the kernel. Low latency when wait time is tiny; wastes CPU and can degrade system throughput when hold times are long. Often implemented with `atomic` loads + `pause`/`yield`. → [Part 1.5](../01-pthreads/05-semaphores-and-spinlocks.md)

**cache line** — The unit of cache coherency (typically 64 bytes): when one core writes a line, other cores' copies are invalidated. Adjacent unrelated variables on the same line cause **false sharing**. → [Part 5.4](../05-pitfalls/04-false-sharing.md)

**CAS (compare-and-swap)** — Atomic read-modify-write: if the location equals `expected`, store `desired`; else update `expected` with the current value. Foundation of lock-free data structures; subject to the ABA problem on bare pointers. → [Part 3.2](../03-atomics-and-memory-model/02-lock-free-and-cas.md)

**condition variable** — Lets a thread block until a **predicate** on shared state becomes true, paired with a mutex. `wait` atomically releases the mutex and sleeps; `notify` wakes waiters who re-check the predicate in a loop. → [Part 1.3](../01-pthreads/03-condition-variables.md) · [Part 2.3](../02-cpp-threads/03-condition-variables.md)

**context switch** — The kernel saving one thread's registers and restoring another's so a different thread runs on a core. Costs microseconds and flushes hot cache state; motivation for pooling threads and avoiding oversubscription. → [Part 0.3](../00-foundations/03-threads-and-the-os.md)

**concurrency** — Structural ability for multiple tasks to make progress without each blocking the others; on one core this is **interleaving** via time-slicing. Distinct from true simultaneous execution. → [Part 0.2](../00-foundations/02-concurrency-vs-parallelism.md)

**critical section** — Code that accesses shared mutable state and must not run concurrently with other critical sections on the same data. Guard with mutex, rwlock, or atomic protocol. Keep it small. → [Part 0.4](../00-foundations/04-the-shared-state-problem.md)

**data race** — Two unsynchronized accesses to the same memory location, at least one a write, with no happens-before between them — **undefined behavior** in C/C++. Not the same colloquial "race condition" (logical bug). → [Part 0.4](../00-foundations/04-the-shared-state-problem.md) · [Part 5.1](../05-pitfalls/01-race-conditions.md)

**deadlock** — A set of threads blocked forever, each waiting for a resource held by another; requires circular wait (among other conditions). Classic example: dining philosophers grabbing forks in cyclic order. → [Part 5.2](../05-pitfalls/02-deadlock.md) · [Part 4.4](../04-patterns/04-dining-philosophers.md)

**detach** — Mark a thread as not **join**able: its resources are reclaimed automatically at exit, but the parent cannot retrieve its return value. Leaked OS threads if the process exits while detached threads still run. → [Part 1.1](../01-pthreads/01-thread-lifecycle.md) · [Part 2.1](../02-cpp-threads/01-std-thread.md)

**false sharing** — Performance degradation when threads write different variables that share a cache line, causing needless coherency traffic. Fix with padding or alignment to separate hot per-thread fields. → [Part 5.4](../05-pitfalls/04-false-sharing.md)

**fence (memory fence)** — An explicit ordering constraint (`atomic_thread_fence`) that orders other memory operations without necessarily touching a particular atomic object. Lower-level than acquire/release on a specific atomic. → [Part 3.5](../03-atomics-and-memory-model/05-fences-and-reordering.md)

**futex** — Linux "fast userspace mutex": atomic userspace check plus kernel wait only on contention. Underpins pthread mutexes and C++ mutex implementations on glibc. → [Part 0.3](../00-foundations/03-threads-and-the-os.md)

**happens-before** — A partial order between operations: if A happens-before B, B observes A's effects. Created by mutex lock/unlock, thread join, atomic release/acquire, etc. The correctness backbone of the memory model. → [Part 3.3](../03-atomics-and-memory-model/03-the-memory-model.md)

**hardware_concurrency** — `std::thread::hardware_concurrency()` — hint of logical processor count (cores × SMT). Useful default for thread pool sizing; zero means unknown. → [Part 4.1](../04-patterns/01-thread-pool.md)

**join** — Block until a thread finishes and optionally collect its return value / exit status. Ensures thread resources are reclaimed in a defined order. → [Part 1.1](../01-pthreads/01-thread-lifecycle.md) · [Part 2.1](../02-cpp-threads/01-std-thread.md)

**livelock** — Threads remain runnable and react to each other but make no useful progress (e.g. polite backoff collisions). Differs from deadlock (no thread blocked) but equally bad for throughput. → [Part 5.3](../05-pitfalls/03-livelock-and-starvation.md)

**lock-free** — A progress guarantee: system-wide, at least one thread makes progress in finite steps; individual threads may starve. Typically built from CAS loops; harder than mutexes and still needs memory-order discipline. → [Part 3.2](../03-atomics-and-memory-model/02-lock-free-and-cas.md)

**lost wakeup** — A thread misses a `notify` because it was not yet waiting, or because state changed between check and sleep without holding the lock correctly. Prevented by predicate loops and holding the mutex across check→wait. → [Part 5.6](../05-pitfalls/06-lost-wakeups.md)

**memory model** — The rules defining which values cross-thread reads may observe and which reorderings compilers and CPUs may perform. C++11 and C11 define formal models; pre-standard code had no portable concurrency semantics. → [Part 3.3](../03-atomics-and-memory-model/03-the-memory-model.md)

**memory order** — Modifiers on atomic operations (`relaxed`, `acquire`, `release`, `acq_rel`, `seq_cst`) trading synchronization strength for performance. Incorrect ordering produces subtle bugs, not just "stale values." → [Part 3.4](../03-atomics-and-memory-model/04-memory-orders.md)

**mutex** — Mutual exclusion lock: at most one thread holds it; others block until release. Establishes happens-before between critical sections. Default first choice for protecting invariants. → [Part 1.2](../01-pthreads/02-mutexes.md) · [Part 2.2](../02-cpp-threads/02-mutexes-and-locks.md)

**parallelism** — Simultaneous execution of multiple instruction streams on separate cores or hardware threads. Requires hardware; not implied by concurrency alone. → [Part 0.2](../00-foundations/02-concurrency-vs-parallelism.md)

**predicate** — A condition on shared state that must be true before a waiting thread proceeds (e.g. `!queue.empty()`). Condition variables require re-testing the predicate after every wakeup. → [Part 1.3](../01-pthreads/03-condition-variables.md) · [Part 4.2](../04-patterns/02-producer-consumer.md)

**priority inversion** — A high-priority thread blocked behind a low-priority thread that holds a lock, while a medium-priority thread preempts the low one — indirect blocking. OS priority inheritance mutexes mitigate on some platforms. → [Part 0.3](../00-foundations/03-threads-and-the-os.md)

**producer-consumer** — Pattern decoupling item generation from processing via a thread-safe queue; bounded capacity applies back-pressure. Foundation of pipelines and thread pool task queues. → [Part 4.2](../04-patterns/02-producer-consumer.md)

**race condition** — Colloquial: a bug from timing-dependent interleaving (may occur even without a standard data race, e.g. two atomic ops forming a logical race). In standard terms, distinguish from **data race** (UB). → [Part 5.1](../05-pitfalls/01-race-conditions.md)

**reader-writer lock** — Allows many concurrent readers OR one writer. Wins on read-heavy, non-trivial critical sections; loses to atomics for word-sized data and to plain mutexes under low contention. → [Part 4.3](../04-patterns/03-readers-writers.md)

**relaxed ordering** — Atomic operation with no cross-thread ordering guarantees — only atomicity of that location. Fast for counters and statistics where no other memory publishes through the atomic. → [Part 3.4](../03-atomics-and-memory-model/04-memory-orders.md)

**RAII lock** — Scope-bound lock acquisition (`lock_guard`, `unique_lock`, `scoped_lock`) so unlock happens on every exit path including exceptions. Eliminates forgotten unlocks and double-lock bugs. → [Part 2.2](../02-cpp-threads/02-mutexes-and-locks.md)

**semaphore** — Counter-based synchronization: `wait` decrements (blocking at zero), `post`/`release` increments. Used to limit concurrent access to N resources (e.g. N−1 dining philosophers). → [Part 1.5](../01-pthreads/05-semaphores-and-spinlocks.md) · [Part 2.6](../02-cpp-threads/06-cpp20-concurrency.md)

**sequential consistency** — Strongest C++ memory order: all `seq_cst` atomics appear in a single total order consistent with program order. Default for atomics; simplest to reason about; often slower than acquire/release. → [Part 3.4](../03-atomics-and-memory-model/04-memory-orders.md)

**spinlock** — Mutex implemented by busy-waiting on an atomic flag. Appropriate only when contention is rare and hold time is shorter than a context switch. → [Part 1.5](../01-pthreads/05-semaphores-and-spinlocks.md)

**spurious wakeup** — A condition variable may return from `wait` without a matching `notify`; predicate loops make this harmless. POSIX and C++ both permit spurious wakeups. → [Part 1.3](../01-pthreads/03-condition-variables.md) · [Part 5.6](../05-pitfalls/06-lost-wakeups.md)

**starvation** — A thread never obtains the resource it needs despite system progress (e.g. writers blocked forever under reader-preference rwlocks). Related to fairness policy of the synchronizer. → [Part 5.3](../05-pitfalls/03-livelock-and-starvation.md)

**thread** — An independent execution stream sharing its process's address space (heap, globals, code) while owning its own stack and registers. Scheduled by the OS as a kernel task. → [Part 0.1](../00-foundations/01-what-is-a-thread.md)

**thread pool** — Fixed set of worker threads pulling tasks from a shared (or per-worker) queue; amortizes creation cost and bounds concurrency. → [Part 4.1](../04-patterns/01-thread-pool.md)

**thread-local storage (TLS)** — Per-thread copies of data (`thread_local`, `pthread_setspecific`) — not shared, therefore not subject to data races among threads on that variable. → [Part 1.6](../01-pthreads/06-tls-and-cancellation.md) · [Part 2.5](../02-cpp-threads/05-call-once-and-thread-local.md)

**TSO (total store order)** — x86's memory model: stores are not reordered with each other (from each CPU's view); loads may be reordered with earlier stores to *different* addresses. Explains why naive code "works" on x86 but breaks on ARM. → [Part 3.5](../03-atomics-and-memory-model/05-fences-and-reordering.md)

**work-stealing** — Scheduling strategy with per-worker deques: owner pops LIFO from one end, idle workers steal FIFO from the other. Balances recursive fork-join load with low contention. → [Part 4.6](../04-patterns/06-work-stealing.md)

---

Back to [README](../README.md)
