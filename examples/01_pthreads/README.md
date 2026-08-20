# POSIX Threads (pthreads)

The pthread library is the C-level threading API on every Unix-like system
(Linux, macOS, BSD). Even when you write C++, knowing pthreads helps because
`std::thread` is implemented on top of it on those platforms.

```
   +-----------------------------------------------------------+
   |                       libpthread.so                       |
   |  +--------+  +---------+  +---------+  +---------------+  |
   |  | thread |  |  mutex  |  | cond_var|  | rwlock/barrier|  |
   |  +--------+  +---------+  +---------+  +---------------+  |
   |       wraps clone()/futex() syscalls on Linux             |
   +-----------------------------------------------------------+
                              |
                              v
   +-----------------------------------------------------------+
   |  Linux kernel: schedules tasks (threads ARE light tasks)  |
   +-----------------------------------------------------------+
```

## Cheat sheet

| Object              | Type                  | Create / init                   | Destroy                 |
|---------------------|-----------------------|---------------------------------|-------------------------|
| Thread              | `pthread_t`           | `pthread_create`                | `pthread_join` / `_detach` |
| Mutex               | `pthread_mutex_t`     | `pthread_mutex_init` or `PTHREAD_MUTEX_INITIALIZER` | `pthread_mutex_destroy` |
| Condition variable  | `pthread_cond_t`      | `pthread_cond_init` or `PTHREAD_COND_INITIALIZER` | `pthread_cond_destroy` |
| Read/write lock     | `pthread_rwlock_t`    | `pthread_rwlock_init`           | `pthread_rwlock_destroy`|
| Barrier             | `pthread_barrier_t`   | `pthread_barrier_init`          | `pthread_barrier_destroy`|
| Spin lock           | `pthread_spinlock_t`  | `pthread_spin_init`             | `pthread_spin_destroy`  |
| Semaphore (POSIX)   | `sem_t`               | `sem_init`                      | `sem_destroy`           |
| TLS key             | `pthread_key_t`       | `pthread_key_create`            | `pthread_key_delete`    |

## Subdirectories

1. `01_basics/` - create, join, detach, pass arguments, return values
2. `02_mutex/` - regular, recursive, trylock, timed, deadlock demo
3. `03_condition_variables/` - signal, broadcast, producer/consumer
4. `04_rwlock/` - many readers, one writer
5. `05_barriers/` - synchronize N threads at a point
6. `06_semaphores/` - counting semaphore for resource management
7. `07_spinlock/` - busy-wait short critical sections
8. `08_thread_specific/` - per-thread storage with `pthread_key_t`
9. `09_cancellation/` - cooperative thread termination

## Build

```
make            # build everything
make run        # run them all
make clean
```
