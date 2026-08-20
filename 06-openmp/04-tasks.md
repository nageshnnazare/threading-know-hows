# 6.4 — Tasks

**Loop worksharing** (Part 6.2) excels when iteration space is regular and known.
**Tasks** handle **irregular**, **recursive**, and **pointer-chasing** parallelism —
a thread encounters `#pragma omp task`, enqueues work, and any idle team member
may execute it. This chapter covers task creation, synchronization, dependences,
and the single-producer pattern with a recursive cutoff.

---

## 6.4.1 `#pragma omp task` vs parallel for

```
   parallel for                task
   ─────────────               ────
   iterations known upfront    work discovered at runtime
   static/dynamic schedule     queue + work-stealing-ish runtime
   flat loop index             trees, graphs, recursive divide
```

```c
#pragma omp parallel
{
    #pragma omp single      /* one thread produces tasks */
    {
        #pragma omp task
        do_phase_A();

        #pragma omp task
        do_phase_B();
    }
    #pragma omp taskwait      /* all tasks from this team done */
}
```

Each **`task`** generates a **task instance** stored in a runtime queue. Threads
not busy in the parallel region **dequeue** tasks — similar spirit to work-stealing
(Part 4.6) but managed by the OpenMP runtime.

> **The API ▸**
> ```c
> #pragma omp task [shared(v) private(v) firstprivate(v) ...]
> #pragma omp taskwait          /* wait for child tasks of this task */
> #pragma omp taskgroup         /* wait for all tasks spawned in this block */
> ```

---

## 6.4.2 The single-producer pattern

Tasks must be **created by a thread in an active parallel region**. The idiomatic
pattern:

```c
#pragma omp parallel
{
    #pragma omp single   /* exactly one thread enters */
    {
        spawn_root_tasks();
    }
}   /* implicit barrier — all tasks complete before team exits */
```

```
   parallel region (4 threads)
   T0 enters single → creates tasks → helps execute queue
   T1,T2,T3         → blocked at single OR execute tasks from queue
```

Without **`single`**, every thread would duplicate task creation — four copies of
the same subtree.

> **Pitfall ▸** **`#pragma omp master`** instead of **`single`** does **not**
> imply a barrier — other threads may exit the parallel region before tasks finish.
> Use **`single`** + implicit/explicit sync, or **`taskwait`**.

---

## 6.4.3 `taskwait` and `taskgroup`

| Directive | Waits for |
|-----------|-----------|
| **`taskwait`** | Child tasks of **current** task |
| **`taskgroup`** | All tasks spawned in the **current block** (including nested) |

```c
#pragma omp parallel
#pragma omp single
{
    #pragma omp task
    foo();

    #pragma omp taskgroup
    {
        #pragma omp task
        bar();
        #pragma omp task
        baz();
    }   /* bar and baz done */

    #pragma omp taskwait   /* any other children of this task */
}
```

Use **`taskgroup`** when a subtree must complete before continuing (e.g. recursive
join point).

---

## 6.4.4 Task dependences (OpenMP 4.0+)

Express **DAG edges** between tasks without manual barriers:

```c
#pragma omp parallel
#pragma omp single
{
    int a = 0, b = 0, c = 0;

    #pragma omp task shared(a) depend(out:a)
    a = produce_a();

    #pragma omp task shared(b) depend(out:b)
    b = produce_b();

    #pragma omp task shared(a,b,c) depend(in:a,b) depend(out:c)
    c = a + b;

    #pragma omp task shared(c) depend(in:c)
    consume(c);
}
```

```
   task(A) ──┐
             ├──▶ task(C) ──▶ task(consume)
   task(B) ──┘

   runtime will not start C until A and B complete
```

**Trade-offs ▸** Dependences add scheduling overhead but replace ad-hoc **`critical`**
flags for pipeline stages. Best when task count is moderate and edges are sparse.

---

## 6.4.5 Recursive example: parallel Fibonacci with cutoff

Naive parallel Fibonacci spawns **2^n** tasks — catastrophic overhead. Production
code uses a **cutoff**: below threshold, compute serially.

```c
// gcc -fopenmp fib_tasks.c -o fib_tasks
#ifdef _OPENMP
#include <omp.h>
#endif
#include <stdio.h>

#define CUTOFF 20

long fib_serial(int n) {
    if (n < 2) return n;
    return fib_serial(n - 1) + fib_serial(n - 2);
}

long fib_task(int n) {
    if (n < CUTOFF)
        return fib_serial(n);

    long a, b;
    #pragma omp task shared(a) firstprivate(n)
    a = fib_task(n - 1);

    #pragma omp task shared(b) firstprivate(n)
    b = fib_task(n - 2);

    #pragma omp taskwait
    return a + b;
}

int main(void) {
    long result = 0;
    #pragma omp parallel
    {
        #pragma omp single
        result = fib_task(40);
    }
    printf("fib(40) = %ld\n", result);
    return 0;
}
```

Execution tree (conceptual):

```
   fib(40)
   ├── task fib(39)
   │   ├── task fib(38) ...
   │   └── task fib(37) ...
   └── task fib(38) ...
   taskwait → combine

   below CUTOFF=20: serial recursion (no task spawn)
```

> **Under the hood ▸** The runtime maintains **deques** per thread; creating threads
   may **steal** tasks from busy threads' queues (implementation-defined, similar
   to Part 4.6). **`taskwait`** suspends the current task until children finish —
   the thread can execute other tasks while waiting.

---

## 6.4.6 Tree traversal pattern

Same structure as Fibonacci — common for ASTs, scene graphs, sparse matrices:

```c
void traverse(Node* node) {
    if (!node) return;
    process(node);

    #pragma omp task if(node->size > THRESH) shared(node)
    traverse(node->left);

    #pragma omp task if(node->size > THRESH) shared(node)
    traverse(node->right);

    #pragma omp taskwait
}
```

The **`if`** clause suppresses task creation for small subtrees — essential for
performance.

```c
#pragma omp task if(should_parallelize)
```

When `if(0)`, the task executes **immediately** by the encountering thread
(merged into parent).

---

## 6.4.7 Tasks vs threads — when to stop

```
   fib(10) parallel     →  slower than serial (task overhead dominates)
   fib(40) + cutoff     →  may win on many cores
   linked list walk     →  tasks rarely beat flat loop + pool
   unbalanced tree      →  tasks shine with cutoff + dynamic scheduling
```

**Rule ▸** Profile. OpenMP tasks carry creation/scheduling cost (~100s of ns each).
Use them when **available parallelism >> thread count** and work is **discovered
dynamically**. Otherwise prefer **`parallel for`** (Part 6.2) or a **thread pool**
(Part 4.1).

For races on shared task inputs, apply Part 5.1 rules — **`shared`** is default;
protect mutable globals with **`critical`** or redesign to **`firstprivate`** copies.

---

## Summary

- **`#pragma omp task`** — dynamic work units for irregular/recursive parallelism;
   loop worksharing (Part 6.2) for regular iteration.
- **`single`** — one producer creates tasks; team members execute from queue.
- **`taskwait`** / **`taskgroup`** — synchronize child task completion.
- **`depend(in/out/inout)`** — DAG scheduling without manual flags.
- Always use a **cutoff** / **`if`** clause — naive recursive task spawn is slower
  than serial; Fibonacci and tree walk are teaching patterns, not end goals.

Next: [API cheat sheet](../99-reference/api-cheatsheet.md)
