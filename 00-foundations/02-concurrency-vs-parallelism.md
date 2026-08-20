# 0.2 — Concurrency vs Parallelism

These two words are used interchangeably in casual conversation. In systems
programming they mean **different things**, and confusing them leads to wrong
expectations about speedup, hardware requirements, and synchronization design.

```
   CONCURRENCY                         PARALLELISM
   ───────────                         ───────────
   structure: many tasks               execution: many tasks
   making progress at once             running at the same instant
   (interleaved on 1 core is OK)       (requires multiple cores / HW threads)
```

Part 0.1 introduced threads as shared-memory execution streams. This chapter
pins down *why* you might spawn them — and what hardware you actually need for
each goal.

---

## 0.2.1 Concurrency: dealing with many things

**Concurrency** is about *structure*: your program is organized so that
multiple logical tasks can advance without each one blocking the others.

```
   one core, time-sliced:

   time ──────────────────────────────────────────────▶

   task A:  ████░░░░░░████░░░░░░████
   task B:  ░░░░████░░░░░░████░░░░░░
   task C:  ░░░░░░░░████░░░░░░████░░

   ░ = another task runs    █ = this task runs
```

On a **single core**, the OS scheduler gives each runnable thread a **time
slice** (typically 1–10 ms on Linux, tunable via `sched_rr`). Threads appear
to run "at the same time" because they switch fast enough — but at any instant
only **one** thread executes user code on that core.

> **Under the hood ▸** Time-slicing is implemented by a periodic timer interrupt
> (or equivalent). When the slice expires, the kernel's scheduler picks the
> next runnable thread, saves the current thread's registers to its kernel
> stack, restores the next thread's registers, and resumes. That is a
> **context switch** (Part 0.3). Concurrency on one core costs you those
> switches but buys responsiveness: a blocked I/O thread does not stall the
> whole process.

**Trade-offs ▸** Concurrency improves **latency** for interactive or I/O-bound
work (a UI stays responsive while a download runs) without requiring extra
hardware. It does **not** by itself make CPU-bound work finish sooner on one
core — you still execute one instruction stream at a time.

---

## 0.2.2 Parallelism: doing many things at once

**Parallelism** is about *execution*: two or more instruction streams truly
execute **simultaneously** on separate execution units.

![Concurrency is interleaving; parallelism is simultaneous execution](figures/concurrency-vs-parallelism.svg)

```
   four cores, four threads — real overlap:

   core 0:  ████████████████████████████  thread A
   core 1:  ████████████████████████████  thread B
   core 2:  ████████████████████████████  thread C
   core 3:  ████████████████████████████  thread D

   all four run at the same wall-clock instant
```

Parallelism requires **multiple independent execution pipelines** — physical
cores, or **hardware threads** (SMT / Hyper-Threading) that share a core's
execution units but have separate register files and can run different
instruction streams when the other is stalled.

> **Under the hood ▸** A modern CPU core is not one simple pipeline. It has
> multiple ALUs, load/store units, and out-of-order execution. **Simultaneous
> Multithreading (SMT)** — Intel's Hyper-Threading, AMD's SMT — exposes two
> logical CPUs per physical core. They share the L1/L2 cache and most execution
> units but have separate architectural state (registers, PC). Throughput gain
> is typically **1.2–1.3× per core**, not 2×, because the shared units still
> bottleneck. Treat SMT threads as "bonus" parallelism, not full extra cores.

**Trade-offs ▸** Parallelism improves **throughput** for embarrassingly
parallel or well-partitioned compute. It does nothing for a strictly serial
algorithm on one core, and it introduces the shared-state problems of Part 0.4
because threads now touch memory at the same instant.

---

## 0.2.3 Cores vs hardware threads vs software threads

Keep three layers straight:

```
   your program          OS / kernel              hardware
   ────────────          ───────────              ─────────

   N pthreads      →     M runnable tasks    →    P physical cores
   (software               (1:1 on Linux)          × SMT factor
    threads)                                        = logical CPUs
```

| Layer | What it is | Typical count |
|-------|------------|---------------|
| Software thread | `pthread_t` you create | dozens to thousands |
| Kernel task | What the scheduler runs | 1:1 with pthread on Linux |
| Physical core | Independent execution pipeline | 4–128+ on servers |
| Logical CPU | What `top` shows (`nproc`) | cores × SMT (often 2×) |

> **Rule ▸** You can have **more software threads than cores** (concurrency +
> parallelism mixed). You cannot run more threads *in parallel* than you have
> logical CPUs without time-slicing. Spawning 10 000 threads on 8 cores means
> most are waiting in the run queue, not executing.

---

## 0.2.4 Latency vs throughput

| Goal | Metric | Concurrency helps? | Parallelism helps? |
|------|--------|--------------------|--------------------|
| UI stays responsive while loading | **Latency** (time to react) | ✓ strongly | optional |
| Render 10 000 frames | **Throughput** (work/sec) | weakly | ✓ strongly |
| Serve 50 000 connections | **Throughput** | ✓ (async I/O) | ✓ (multi-core accept) |

A single-core server can be **concurrent** (many connections interleaved via
`epoll` and non-blocking I/O) without being **parallel**. A 64-core batch job
is **parallel** but may have no concurrency requirement at all if each core runs
one long task.

---

## 0.2.5 Amdahl's law: the serial fraction caps speedup

Even with infinite cores, a program cannot speed up beyond the fraction of work
that is **inherently serial** (initialization, a global reduction, lock
contention):

```
   speedup(N) = 1 / ( S + (1 − S) / N )

   S = serial fraction (0 = all parallel, 1 = all serial)
   N = number of parallel workers
```

```
   S = 5% serial, N = 8 cores:

   speedup = 1 / (0.05 + 0.95/8) ≈ 4.7×     (not 8×)

   S = 5% serial, N → ∞:

   speedup → 1 / 0.05 = 20×                  (hard ceiling)
```

> **Pitfall ▸** A tiny serial section — one global mutex around every
> `count++`, a single-threaded sort at the end — dominates at scale. Profile
> before adding cores; the bottleneck may be **Amdahl's S**, not lack of
> parallelism.

**Gustafson's law** reframes the question: if the problem size grows with
hardware (more data → more parallel work), you can achieve near-linear scaling
even with a fixed serial fraction — because the serial part becomes a smaller
*proportion* of the total. Amdahl assumes fixed problem size; Gustafson assumes
fixed time and growing work. Both are true in different domains; Amdahl is the
conservative bound for "same job, more cores."

---

## 0.2.6 Putting it together

```
   need responsiveness on 1 core?     → concurrency (time-slicing, async I/O)
   need more compute per second?     → parallelism (multiple cores)
   need both?                        → threads + sensible task count ≈ cores
```

Remember the guide's core rule (README): shared mutable state accessed
concurrently without synchronization is undefined behavior. Parallelism makes
races **more likely** (true overlap); concurrency makes them **harder to
reproduce** (timing-dependent). Either way, the fix is the same — mutexes
(Part 1.2) or atomics (Part 3.1).

---

## Summary

- **Concurrency** = structure; many tasks making progress, interleaved on one
  core via time-slicing — good for responsiveness and I/O.
- **Parallelism** = simultaneous execution on multiple cores or SMT hardware
  threads — good for compute throughput.
- Software threads (pthread) map 1:1 to kernel tasks on Linux; logical CPUs =
  cores × SMT.
- **Latency** vs **throughput** determine which goal matters; they need
  different designs.
- **Amdahl's law**: serial fraction S caps speedup at 1/S; even 5% serial
  limits an 8-core run to ~4.7×. Gustafson relaxes this when problem size
  scales with hardware.

Next: [0.3 — Threads and the OS](03-threads-and-the-os.md)
