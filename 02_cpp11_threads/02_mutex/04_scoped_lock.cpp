// 04_scoped_lock.cpp -- C++17's std::scoped_lock: lock_guard for many mutexes.
//
//  Locks ALL its mutex args ATOMICALLY (no deadlock) using a deadlock-
//  avoidance algorithm under the hood (try-and-back-off, like std::lock).
//
//      std::scoped_lock g(m1, m2, m3);     // can never deadlock between these
//
//  Picture: two threads transferring money between accounts.
//
//      thread 1:                   thread 2:
//      transfer(A, B, 10)          transfer(B, A, 5)
//
//      with naive `lock(A); lock(B);`:
//          T1 holds A, wants B; T2 holds B, wants A -> DEADLOCK
//
//      with std::scoped_lock(m1, m2):
//          both threads lock both, no fixed order needed,
//          deadlock impossible.
//
//  Differences:
//
//      lock_guard      one mutex
//      scoped_lock     0..N mutexes (C++17), prevents deadlock between them

#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

struct Account {
    int             balance;
    mutable std::mutex m;
};

static void transfer(Account& from, Account& to, int amount)
{
    std::scoped_lock g(from.m, to.m);            // atomic two-mutex lock
    from.balance -= amount;
    to.balance   += amount;
}

int main()
{
    Account A{100, {}}, B{100, {}};

    std::vector<std::thread> ts;
    for (int i = 0; i < 1000; ++i) {
        ts.emplace_back([&]{ transfer(A, B, 1); });
        ts.emplace_back([&]{ transfer(B, A, 1); });
    }
    for (auto& t : ts) t.join();

    std::cout << "A=" << A.balance << "  B=" << B.balance
              << "  total=" << (A.balance + B.balance) << "\n";
}
