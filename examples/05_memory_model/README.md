# The C++ Memory Model

Why does any of this matter? Because compilers and CPUs are AGGRESSIVE
optimizers and reorderers. Your code:

```
data = 42;
flag = true;
```

may be reordered to `flag = true; data = 42;` either by the compiler (e.g.
register allocation) or by the CPU (e.g. store buffer drain order). For
SINGLE-threaded code that's fine because the as-if rule guarantees the
observable behavior. For MULTI-threaded code, another thread observing
`flag` first would race against `data`.

Memory orders constrain those reorderings to give you well-defined
inter-thread visibility.

```
                 strict <-------------------> permissive
                 SLOW                              FAST

   seq_cst   ->   acq_rel   ->   acquire / release   ->   relaxed

   Total order   "happens-      "happens-before for     atomicity only,
   visible to    before for     a single sync pair"     no ordering.
   all threads   the RMW pair"
```

## Files

| #  | File                               | Topic |
|----|------------------------------------|-------|
| 01 | `01_seq_cst.cpp`                   | Default; the simplest mental model |
| 02 | `02_acquire_release.cpp`           | The workhorse pattern |
| 03 | `03_relaxed.cpp`                   | Counters where ordering doesn't matter |
| 04 | `04_release_consume.cpp`           | The (deprecated) consume order |
| 05 | `05_fences.cpp`                    | Standalone fences |
| 06 | `06_store_load_reorder.cpp`        | Demonstrating CPU reordering |

Useful resource: [cppreference: memory_order](https://en.cppreference.com/w/cpp/atomic/memory_order)

Build: `make`
