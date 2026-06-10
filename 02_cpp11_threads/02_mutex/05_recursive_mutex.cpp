// 05_recursive_mutex.cpp -- std::recursive_mutex.
//
//  Like std::mutex, except the SAME thread can lock it multiple times.
//  An internal counter tracks the depth; you must unlock as many times
//  as you locked.
//
//      thread A: m.lock();   // count 1
//      thread A: m.lock();   // count 2  (no block!)
//      thread A: m.unlock(); // count 1
//      thread A: m.unlock(); // count 0 (released)
//
//  Useful for re-entrant code where one method calls another that also
//  locks the same mutex. But: many designers consider this a code smell --
//  redesign so that public methods lock and call private helpers that
//  assume the lock is held.

#include <iostream>
#include <mutex>
#include <thread>

class Tree {
    std::recursive_mutex m_;
    int                  count_ = 0;

    void recurse(int depth) {
        std::lock_guard<std::recursive_mutex> g(m_);
        ++count_;
        if (depth > 0) recurse(depth - 1);
    }
public:
    void run(int depth) { recurse(depth); }
    int  count() const  {
        // const_cast because the mutex is logically mutable;
        // in real code make m_ `mutable` instead.
        return count_;
    }
};

int main()
{
    Tree t;
    std::thread th([&]{ t.run(5); });
    th.join();
    std::cout << "count = " << t.count() << "\n";
}
