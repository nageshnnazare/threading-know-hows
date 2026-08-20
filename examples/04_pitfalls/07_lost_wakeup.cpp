// 07_lost_wakeup.cpp -- Why you MUST always recheck the predicate in a loop.
//
//  THE BUG (without predicate-loop):
//
//      consumer                       producer
//      --------                       --------
//      check `ready` (false)
//                                     lock(m)
//                                     ready = true
//                                     notify_one()
//                                     unlock(m)
//      cv.wait(...)   <-- sleeps FOREVER, the wake-up was already missed
//
//  Even if you DO use the loop without the lock around predicate-set, this
//  same race exists. Hence the rule:
//
//      Set predicate AND notify while holding the mutex; consume the
//      predicate inside the same mutex.
//
//  Fix: cv.wait(g, [&]{ return ready; });
//      - cv.wait first checks the predicate while holding `g`. If true,
//        it returns immediately without sleeping.
//      - Else atomically (mutex-released + sleep), then on wake re-acquires
//        and rechecks.
//
//  This file shows BOTH versions. The buggy one triggers reliably with a
//  small sleep added by the consumer between read and wait.

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

std::mutex              m;
std::condition_variable cv;
bool                    ready = false;

void buggy_consumer()
{
    if (!ready) {
        // Simulate scheduling that lets producer run "between" the check
        // and the wait.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::unique_lock<std::mutex> g(m);
        cv.wait(g);    // No predicate -> may sleep forever if wakeup happened above.
    }
    std::cout << "buggy consumer woken (ready=" << ready << ")\n";
}

void good_consumer()
{
    std::unique_lock<std::mutex> g(m);
    cv.wait(g, []{ return ready; });    // safe regardless of timing
    std::cout << "good consumer woken (ready=" << ready << ")\n";
}

int main()
{
    /* Demonstrate the GOOD version (which is robust). */
    ready = false;
    std::thread c(good_consumer);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    {
        std::lock_guard<std::mutex> g(m);
        ready = true;
        cv.notify_all();
    }
    c.join();

    /* Demonstrating the bug deterministically across schedulers is hard;
     * the takeaway is: ALWAYS use the predicate-loop form. The buggy
     * function is shown above for instruction only. */
}
