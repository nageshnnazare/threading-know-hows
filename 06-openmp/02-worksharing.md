# 6.2 — Worksharing: for & sections

A **parallel region** (Part 6.1) gives you N threads running the **same** code.
**Worksharing** divides **iterations** or **code sections** among them so each
thread does different work. This chapter covers `#pragma omp for`, scheduling,
`sections`, and the `single` / `master` roles.

---

## 6.2.1 `#pragma omp for` — loop iteration distribution

The combined form is idiomatic:

```c
#pragma omp parallel for
for (int i = 0; i < N; ++i) {
    a[i] = f(i);
}
```

Equivalent to:

```c
#pragma omp parallel
{
    #pragma omp for
    for (int i = 0; i < N; ++i) {
        a[i] = f(i);
    }
}
```

Picture — 12 iterations, 4 threads, default static schedule:

```
   iterations:  0  1  2 | 3  4  5 | 6  7  8 | 9 10 11
   thread:      T0       T1        T2        T3
```

Each thread owns a **contiguous chunk** (static) unless you choose otherwise.

> **The API ▸**
> ```c
> #pragma omp for schedule(kind[, chunk]) private(...) reduction(...)
> ```
> The loop must be **canonical** — countable `for`, signed/unsigned index, bounds
> known at entry. C++11 range-for is not directly parallelizable without index loops.

---

## 6.2.2 Schedule kinds — when to use each

```
   schedule(static)       divide at region start — zero runtime overhead
   schedule(static, 1)    round-robin by iteration
   schedule(dynamic, k)   threads grab chunks of k from a queue
   schedule(guided)       chunks shrink over time (large → small)
   schedule(auto)         implementation picks
   schedule(runtime)      read OMP_SCHEDULE env var
```

| Schedule | Best when | Risk |
|----------|-----------|------|
| **static** | Uniform iteration cost | Last chunk straggler if costs vary |
| **static,1** | Slight imbalance, cache line per iteration | Higher scheduling overhead |
| **dynamic** | **Highly variable** iteration cost | Queue contention at scale |
| **guided** | Decreasing cost toward end of loop | Middle ground |
| **runtime** | Tune without recompile | Must set `OMP_SCHEDULE` |

```c
// gcc -fopenmp schedules.c -o schedules
#ifdef _OPENMP
#include <omp.h>
#include <stdio.h>

int main(void) {
    int N = 12, who[12] = {0};

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; ++i)
        who[i] = omp_get_thread_num();

    printf("static: ");
    for (int i = 0; i < N; ++i) printf("[%d→T%d] ", i, who[i]);
    printf("\n");

    #pragma omp parallel for schedule(dynamic, 2)
    for (int i = 0; i < N; ++i)
        who[i] = omp_get_thread_num();   /* uneven work simulated by id */

    printf("dynamic,2: ");
    for (int i = 0; i < N; ++i) printf("[%d→T%d] ", i, who[i]);
    printf("\n");
}
#else
#include <stdio.h>
int main(void){ puts("Compile with -fopenmp"); return 0; }
#endif
```

---

## 6.2.3 `collapse` for nested loops

Parallelize **multiple** nested loops as one iteration space:

```c
#pragma omp parallel for collapse(2)
for (int i = 0; i < NI; ++i)
    for (int j = 0; j < NJ; ++j)
        a[i][j] = i + j;
```

Without `collapse`, only the **outer** loop is distributed — inner runs serially
per outer iteration. With `collapse(2)`, `(i,j)` pairs are flattened and striped
across threads — better load balance for small outer / large inner.

> **Pitfall ▸** All collapsed loops must be **perfectly nested**, same bounds
> shape, no early `break`/`return` from the inner loop body.

---

## 6.2.4 `nowait` — skip the implicit barrier

`#pragma omp for` ends with an **implicit barrier** unless `nowait`:

```c
#pragma omp parallel
{
    #pragma omp for nowait
    for (int i = 0; i < N; ++i) work_a(i);

    #pragma omp for          /* threads that finished work_a start work_b early */
    for (int i = 0; i < N; ++i) work_b(i);
}   /* final implicit barrier here */
```

**Trade-offs ▸** `nowait` reduces idle time but requires **no data dependence**
between consecutive worksharing constructs unless you synchronize explicitly
(`#pragma omp barrier` — Part 6.3).

---

## 6.2.5 `#pragma omp sections` — task parallelism

When work is **not** a loop — different functions / pipeline stages:

```c
#pragma omp parallel sections
{
    #pragma omp section
    parse_input();

    #pragma omp section
    validate_header();

    #pragma omp section
    prefetch_metadata();
}   /* implicit barrier — all sections done before continuing */
```

```
   parallel region
   ┌─────────┬─────────┬─────────┐
   │ section │ section │ section │
   │   T?    │   T?    │   T?    │   runtime assigns sections to idle threads
   └─────────┴─────────┴─────────┘
```

Each `#pragma omp section` is one **unit of work**. If sections > threads, some
run sequentially in waves. If threads > sections, extra threads idle at the
barrier (unless other work exists).

---

## 6.2.6 `single` and `master`

| Construct | Who runs | Implicit barrier? |
|-----------|----------|-------------------|
| **`#pragma omp single`** | One **unspecified** thread | **Yes** (others wait) unless `nowait` |
| **`#pragma omp master`** | **Thread 0** only | **No** — others don't wait |

```c
#pragma omp parallel
{
    #pragma omp for
    for (int i = 0; i < N; ++i) compute(i);

    #pragma omp single
    {
        printf("reduction prep by thread %d\n", omp_get_thread_num());
    }

    #pragma omp master
    log_only_on_main_thread();   /* other threads race ahead */
}
```

Use **`single`** for one-time setup inside a parallel region (I/O, allocation).
Use **`master`** when only the initial thread may call non-thread-safe APIs.

---

## 6.2.7 Example: parallel loop with speedup check

```c
// gcc -fopenmp -O2 parallel_loop.c -o parallel_loop -lm
#ifdef _OPENMP
#include <omp.h>
#endif
#include <stdio.h>
#include <math.h>

enum { N = 10'000'000 };

int main(void) {
    double sum_serial = 0.0;
    for (int i = 0; i < N; ++i)
        sum_serial += sin(i * 0.000001);

    double sum_parallel = 0.0;
    #pragma omp parallel for reduction(+:sum_parallel)
    for (int i = 0; i < N; ++i)
        sum_parallel += sin(i * 0.000001);

    printf("serial   = %.6f\n", sum_serial);
    printf("parallel = %.6f\n", sum_parallel);
#ifdef _OPENMP
    printf("threads  = %d\n", omp_get_max_threads());
#endif
    return 0;
}
```

`reduction` is detailed in Part 6.3 — it prevents the data race (Part 5.1) that
a naive `sum_parallel += ...` would introduce.

---

## Summary

- **`#pragma omp for`** distributes loop iterations across the team; combine with
  **`parallel`** as `parallel for`.
- **`schedule`**: **static** for uniform work, **dynamic/guided** for variable
  cost; **`collapse(n)`** for nested loops.
- **`nowait`** skips barriers between consecutive loops — only when safe.
- **`sections`** for non-loop task parallelism; **`single`** / **`master`** for
  one-thread duties inside a region.

Next: [6.3 — Synchronization & reductions](03-synchronization-and-reductions.md)
