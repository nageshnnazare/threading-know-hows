# Concurrency Patterns

Higher-level patterns built from the primitives in earlier sections.

| #  | Pattern                  | Use when |
|----|--------------------------|----------|
| 01 | Thread pool              | Many short tasks; bound concurrency to N workers |
| 02 | Producer / consumer      | Decouple producers from consumers via a queue |
| 03 | Readers / writers        | Read-heavy state with infrequent updates |
| 04 | Dining philosophers      | Classic deadlock avoidance demo |
| 05 | Active object            | Serialize access to an object via its own thread |
| 06 | Pipeline                 | Stages connected by queues (e.g. compress -> encrypt -> send) |
| 07 | Work stealing            | Static partitioning + steal-from-others when idle |
| 08 | Double-checked locking   | Lazy init done right (with std::atomic) |

Build: `make`
