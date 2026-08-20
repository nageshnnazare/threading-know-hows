# 6.1 — The OpenMP Model

OpenMP is a **pragma-driven** shared-memory parallel programming model for C, C++,
and Fortran. You annotate serial code with `#pragma omp ...`; the **compiler**
lowers pragmas to calls into an **OpenMP runtime**, which manages a thread team,
work distribution, and barriers. This chapter is the mental model — fork-join,
thread counts, and when OpenMP beats hand-rolled `std::thread` (Part 2.1).

---

## 6.1.1 Fork-join parallelism

OpenMP programs start as **one thread** (the **initial thread**). A parallel
region **forks** a team of threads; at the end, an implicit **barrier joins** back
to one thread:

```
   initial thread (main)
        │
        │  #pragma omp parallel  ← fork
        ├──────┬──────┬──────┐
        T0     T1     T2     T3     (team of 4)
        │      │      │      │
        └──────┴──────┴──────┘     ← implicit barrier (join)
        │
   main continues alone
```

![OpenMP fork-join: one thread spawns a team, then reunites](figures/openmp-fork-join.svg)

Every thread in the team executes **the same block** of code — unlike a thread
pool dispatching different tasks (Part 4.1). Differentiation comes from
`omp_get_thread_num()` and worksharing constructs (Part 6.2).

> **Under the hood ▸** The runtime maintains a **pool of worker threads** (often
> persistent across parallel regions). A parallel region **wakes** existing workers
> rather than creating OS threads from scratch each time — amortizing spawn cost.

---

## 6.1.2 The pragma-driven approach

Contrast with explicit threading:

```
   std::thread / pthread          OpenMP
   ─────────────────────          ──────
   create N thread objects        #pragma omp parallel for
   assign work manually           compiler splits loop iterations
   join each thread               implicit barrier at region end
   ~100 lines for a parallel sum  ~2 lines
```

The compiler inserts:

1. Runtime call to enter parallel region (fork team).
2. Per-thread code (your block).
3. Barrier + team teardown.

You trade **control** for **productivity** — ideal when parallelism maps to
**regular loops** and **numeric kernels**.

**Trade-offs ▸** OpenMP shines in **loop-heavy scientific C/C++**. Irregular
graphs, fine-grained task parallelism, and complex lifetime management often fit
`std::thread` + pools (Part 4) or OpenMP **tasks** (Part 6.4) better.

---

## 6.1.3 Thread identity and team size

> **The API ▸**
> ```c
> #include <omp.h>
> int omp_get_thread_num(void);    // 0 .. num_threads-1 in this team
> int omp_get_num_threads(void);   // team size (valid only inside parallel)
> int omp_get_max_threads(void);   // max this program may use
> void omp_set_num_threads(int n);
> ```
> Build: `gcc -fopenmp file.c -o file` or `g++ -std=c++17 -fopenmp file.cpp -o file`

```c
// gcc -fopenmp hello_omp.c -o hello_omp
#ifdef _OPENMP
#include <omp.h>
#include <stdio.h>

int main(void) {
    printf("max threads = %d\n", omp_get_max_threads());

    #pragma omp parallel
    {
        int id  = omp_get_thread_num();
        int n   = omp_get_num_threads();
        #pragma omp single
        printf("team size = %d\n", n);
        printf("hello from thread %d / %d\n", id, n);
    }
    printf("back on initial thread\n");
    return 0;
}
#else
#include <stdio.h>
int main(void) { puts("Compile with -fopenmp"); return 0; }
#endif
```

> **Pitfall ▸** Calling `omp_get_num_threads()` **outside** a parallel region
> returns **1** — not the team size you expect. Only valid inside `#pragma omp
> parallel`.

---

## 6.1.4 Controlling thread count

Three layers (outer wins at region entry):

```
   1. Environment:  export OMP_NUM_THREADS=8
   2. Runtime API:  omp_set_num_threads(4);
   3. Clause:       #pragma omp parallel num_threads(2)
```

```
   default team size ≈ number of logical cores (implementation-defined)
   OMP_NUM_THREADS=1  →  serial debugging
   num_threads clause →  per-region override
```

> **Rule ▸** Match threads to **physical work**, not "as many as possible."
> Oversubscription (32 threads on 8 cores) hurts when every thread busy-waits
> (Part 0.2, Amdahl).

Nested parallelism (`OMP_NESTED`, `omp_set_nested`) creates sub-teams inside a
parallel region — powerful but easy to **multiply** thread count explosively.

---

## 6.1.5 When OpenMP vs raw threads

| Situation | Prefer |
|-----------|--------|
| Dense `for` loops, stencils, BLAS-like kernels | **OpenMP** |
| Long-lived services, thread pools, I/O multiplexing | **std::thread / pthread** |
| Fine-grained task DAGs, recursive divide-and-conquer | OpenMP **tasks** (Part 6.4) or thread pool |
| Cross-platform GUI / strict C++ RAII lifecycle | **C++ threads** |
| Mixed C legacy + one hot loop | **OpenMP** on the hot loop only |

OpenMP and pthreads **interoperate** on the same process — but don't mix OpenMP
parallel regions with locks held across fork points without care (deadlock risk,
Part 5.2).

---

## 6.1.6 Execution model summary

```
   serial code
   ┌─────────────────────────────────────┐
   │  #pragma omp parallel               │  ← parallel region (league of teams)
   │  {                                  │
   │      #pragma omp for                │  ← worksharing (Part 6.2)
   │      for (...) ...                  │
   │      #pragma omp critical           │  ← sync (Part 6.3)
   │      ...                            │
   │  }                                  │  ← implicit barrier
   └─────────────────────────────────────┘
   serial code
```

Data environment (shared vs private variables) defaults are subtle — Part 6.3
covers clauses. Races on shared loop bounds still happen without proper
synchronization (Part 5.1).

---

## Summary

- OpenMP uses **fork-join**: one thread → team → implicit barrier → one thread.
- **`#pragma omp parallel`** — compiler + runtime create the team; each thread runs
  the same block.
- Query with **`omp_get_thread_num()`** / **`omp_get_num_threads()`**; size via
  **`OMP_NUM_THREADS`**, **`omp_set_num_threads`**, **`num_threads`** clause.
- Best for **loop-heavy numeric C/C++**; raw threads for services, I/O, and
  irregular control flow.
- Build with **`-fopenmp`**; link the OpenMP runtime automatically.

Next: [6.2 — Worksharing: for & sections](02-worksharing.md)
