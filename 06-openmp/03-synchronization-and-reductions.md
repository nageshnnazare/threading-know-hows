# 6.3 — Synchronization & Reductions

Worksharing (Part 6.2) splits iterations; **synchronization** coordinates access
to shared state that worksharing does not privatize. **Reductions** are the
high-performance special case — per-thread private accumulators merged at the end.
This chapter covers OpenMP sync constructs, data-sharing clauses, and the mechanical
story behind `reduction(+:sum)`.

---

## 6.3.1 `critical`, `atomic`, and `barrier`

```
   #pragma omp critical     →  only one thread at a time (named or unnamed)
   #pragma omp atomic       →  atomic RMW on ONE scalar (Part 3.1)
   #pragma omp barrier      →  all threads wait here
   #pragma omp ordered      →  loop iterations execute a block in order
```

| Construct | Scope | Typical cost |
|-----------|-------|--------------|
| **`critical`** | Arbitrary code block | High — serializes like a mutex |
| **`atomic`** | Single assignment/update on scalar | Low — hardware RMW |
| **`barrier`** | Team-wide rendezvous | Moderate — all threads sync |
| **`ordered`** | Per-iteration ordering in a loop | High — partly serial |

```c
#pragma omp parallel for
for (int i = 0; i < N; ++i) {
    #pragma omp atomic
    shared_counter++;          /* OK — single scalar update */

    #pragma omp critical
    {
        log_file[i] = expensive_format(i);   /* arbitrary section */
    }
}
```

> **Pitfall ▸** **`atomic`** applies to **one** memory operation on a **scalar**
> (`int`, `float`, pointer-sized). Multi-field invariants need **`critical`** or
> a proper lock — same rule as Part 5.1.

**Trade-offs ▸** Prefer **`reduction`** over **`atomic`** in a hot loop when the
operation is reducible (`+`, `*`, `max`, …). Reduction privatizes; atomics
contend on every iteration (and may false-share — Part 5.4).

---

## 6.3.2 Data-sharing clauses

Variables in a parallel region have a **data environment**:

```
   #pragma omp parallel private(x) shared(y) firstprivate(z)
```

| Clause | Meaning |
|--------|---------|
| **`shared(v)`** | One variable, all threads see same address (default for file scope) |
| **`private(v)`** | Per-thread uninitialized copy; outer `v` unchanged |
| **`firstprivate(v)`** | Private, initialized from outer value at region start |
| **`lastprivate(v)`** | Private; outer `v` gets last iteration's value at end |
| **`default(none)`** | Force explicit classification — best practice in C++ |
| **`default(shared)`** | C legacy default — dangerous implicit sharing |

```c
int i = 42;
#pragma omp parallel firstprivate(i)
{
    i += omp_get_thread_num();   /* each thread's private copy */
    /* outer i still 42 */
}
```

> **Rule ▸** Use **`default(none)`** and list every variable explicitly. Implicit
> **`shared`** on loop indices and pointers causes subtle races (Part 5.1).

Loop iterators in `#pragma omp for` are **private** by default (OpenMP 3.0+).

---

## 6.3.3 Reduction — mechanical story

![Each thread accumulates privately; runtime combines at barrier](figures/openmp-reduction.svg)

```c
#pragma omp parallel for reduction(+:sum)
for (int i = 0; i < N; ++i)
    sum += a[i];
```

What the runtime does:

```
   1. At parallel region start:
      each thread T gets private sum_T = 0  (identity for +)

   2. During the loop:
      each thread updates only its private sum_T — no cross-thread writes

   3. At implicit barrier before region end:
      sum = sum_T0 + sum_T1 + sum_T2 + ...   (tree combine)
```

Picture (4 threads):

```
   iterations:  [----T0----][----T1----][----T2----][----T3----]
   private:     sum0        sum1        sum2        sum3
   combine:     sum = ((sum0+sum1) + (sum2+sum3))
```

Supported operators: `+`, `*`, `-`, `&`, `|`, `^`, `&&`, `||`, `min`, `max`, …

> **Under the hood ▸** This is exactly the **per-thread accumulator + merge**
> pattern that fixes false sharing (Part 5.4) — OpenMP generates it for you.

---

## 6.3.4 Example: reduction sum

```c
// gcc -fopenmp reduction_sum.c -o reduction_sum
#ifdef _OPENMP
#include <omp.h>
#endif
#include <stdio.h>

int main(void) {
    long N = 1'000'000;
    long sum = 0;
    int  vmax = 0;

    #pragma omp parallel for reduction(+:sum) reduction(max:vmax)
    for (long i = 1; i <= N; ++i) {
        sum += i;
        if ((int)i > vmax) vmax = (int)i;
    }

    printf("sum(1..%ld) = %ld  (expected %ld)\n", N, sum, N * (N + 1) / 2);
    printf("max          = %d\n", vmax);
    return 0;
}
```

Without `reduction(+:sum)`, concurrent `sum += i` is a **data race** (Part 5.1).

---

## 6.3.5 Custom reductions (OpenMP 4.0+)

User-defined combiners for C++ structs:

```cpp
// g++ -std=c++17 -fopenmp custom_reduction.cpp -o custom_reduction
#ifdef _OPENMP
#include <omp.h>
#endif
#include <iostream>

struct Stats { double minv, maxv, count; };

#pragma omp declare reduction(minmax: Stats: \
    omp_out.minv = omp_out.minv < omp_in.minv ? omp_out.minv : omp_in.minv, \
    omp_out.maxv = omp_out.maxv > omp_in.maxv ? omp_out.maxv : omp_in.maxv, \
    omp_out.count += omp_in.count) \
    initializer(omp_priv = {omp_orig.minv, omp_orig.maxv, 0})

int main() {
    Stats g{1e9, -1e9, 0};
    #pragma omp parallel for reduction(minmax:g)
    for (int i = 0; i < 1000; ++i) {
        double v = i * 0.1;
        if (v < g.minv) g.minv = v;
        if (v > g.maxv) g.maxv = v;
        g.count += 1;
    }
    std::cout << "min=" << g.minv << " max=" << g.maxv << " n=" << g.count << "\n";
}
```

The initializer sets each thread's private copy; the combiner merges two privates;
the runtime applies the same tree reduction as built-in ops.

---

## 6.3.6 `ordered` — sequential semantics in parallel loop

```c
#pragma omp parallel for ordered
for (int i = 0; i < N; ++i) {
    compute(i);
    #pragma omp ordered
    printf("done %d\n", i);   /* prints in iteration order 0,1,2,... */
}
```

Only one thread executes the **`ordered`** block at a time, in loop order — useful
for I/O ordering, mostly **anti-parallel**. Avoid in hot paths.

---

## 6.3.7 Choosing a construct

```
   hot loop summing a scalar?     → reduction(op:var)
   occasional shared log write?   → critical (or single thread I/O)
   single counter increment?      → atomic (if reduction not applicable)
   phase boundary?                → barrier (or implicit at region end)
   complex invariant?             → critical / external mutex (Part 1.2)
```

---

## Summary

- **`critical`** serializes arbitrary code; **`atomic`** is one scalar RMW —
  atomics are cheaper but narrower (Part 3.1).
- **`barrier`** team sync; **`ordered`** serializes ordered blocks in loops.
- Data clauses: **`private`**, **`firstprivate`**, **`lastprivate`**, **`shared`**;
  prefer **`default(none)`**.
- **`reduction(op:var)`** — per-thread private copies + tree combine at end;
  avoids races and false sharing (Part 5.1, Part 5.4).
- Custom reductions via **`declare reduction`** for struct aggregates.

Next: [6.4 — Tasks](04-tasks.md)
