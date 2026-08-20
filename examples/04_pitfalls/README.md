# Pitfalls & Their Cures

The classics. Each example shows the BUG, then the FIX, with diagrams.

| #  | Pitfall                  | Symptom                           | Cure |
|----|--------------------------|-----------------------------------|------|
| 01 | Race condition           | Wrong / inconsistent value        | Mutex / atomic |
| 02 | Deadlock                 | Threads stuck forever             | Lock ordering / scoped_lock |
| 03 | Livelock                 | Threads "busy" but no progress    | Backoff / randomization |
| 04 | Starvation               | One thread never runs             | Fair scheduling, no priority inversion |
| 05 | False sharing            | Slow scaling, no obvious cause    | Padding / alignas |
| 06 | ABA problem              | Lock-free CAS appears OK but isn't| Tagged pointers / hazard pointers |
| 07 | Lost wake-up             | cv.notify happens before wait     | Predicate-loop pattern |

Build: `make`. Run with `make run` to see them in action (some
intentionally produce wrong results; the comments call those out).

For a serious tool, run with sanitizers:

```
make CXXFLAGS_EXTRA="-fsanitize=thread -O1 -g"
```
