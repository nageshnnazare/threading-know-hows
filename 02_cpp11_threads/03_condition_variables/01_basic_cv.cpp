// 01_basic_cv.cpp -- std::condition_variable basics.
//
//  Same idea as pthread cond vars (mutex + predicate + wait/notify), but
//  modern C++ adds a HUGE convenience: cv.wait(lock, predicate) wraps the
//  while-loop for you and is robust to spurious wakeups.
//
//      // OLD WAY (still legal)
//      while (!ready) cv.wait(lock);
//
//      // PREFERRED: predicate form
//      cv.wait(lock, []{ return ready; });
//
//  Important rules (same as pthread):
//      - Mutex MUST be locked before wait().
//      - wait() atomically releases mutex while sleeping; reacquires on wake.
//      - notify_one / notify_all should be called after the state change
//        (best practice: also while holding the mutex, then unlock).
//
//  ASCII timeline:
//
//      consumer                    producer
//      --------                    --------
//      lock(m)
//      cv.wait(m, pred) ----+
//          (sleeps)         |
//                           |          lock(m)
//                           |          ready=true
//                           |          cv.notify_one()
//                           |          unlock(m)
//          <-- wakes -------+
//          (re-locks m)
//      use shared state
//      unlock(m)

#include <iostream>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

std::mutex              m;
std::condition_variable cv;
bool                    ready = false;
int                     payload = 0;

void consumer()
{
    std::unique_lock<std::mutex> g(m);
    cv.wait(g, []{ return ready; });           // robust to spurious wakeups
    std::cout << "[consumer] got " << payload << "\n";
}

void producer()
{
    std::this_thread::sleep_for(500ms);
    {
        std::lock_guard<std::mutex> g(m);
        payload = 42;
        ready   = true;
    }
    cv.notify_one();                           // can be done outside the lock
    std::cout << "[producer] notified\n";
}

int main()
{
    std::thread c(consumer), p(producer);
    c.join(); p.join();
}
