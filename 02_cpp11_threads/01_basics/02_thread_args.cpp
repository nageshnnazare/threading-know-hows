// 02_thread_args.cpp -- Passing arguments. Watch the COPY/REFERENCE trap.
//
//  std::thread copies/moves its arguments by VALUE into its own storage --
//  even if the function signature takes a reference!  Use std::ref / std::cref
//  to pass an actual reference.
//
//      void inc(int& n)         { ++n; }
//      int x = 0;
//      std::thread t(inc, x);              // ERROR: x copied; ref to dead copy
//      std::thread t(inc, std::ref(x));    // OK
//
//  Picture (without std::ref):
//
//        main                         thread
//        ----                         ------
//        x = 0                        copy_of_x = 0
//                                     ++copy_of_x  (modifies COPY)
//                                     copy goes out of scope
//        x still == 0  <-- BUG
//
//  With std::ref:
//
//        main                         thread
//        ----                         ------
//        x = 0  --- &x ---->          ref points to x
//                                     ++(*ref)
//        x == 1  <-- OK

#include <iostream>
#include <thread>
#include <functional>     // std::ref
#include <string>

void by_value(int n)            { std::cout << "by_value n="    << n << "\n"; }
void by_ref(int& n)             { ++n; }
void many(const std::string& s, int a, double b)
{
    std::cout << "many: s=\"" << s << "\" a=" << a << " b=" << b << "\n";
}

int main()
{
    int x = 41;

    std::thread t1(by_value, x);            // copies x
    t1.join();

    std::thread t2(by_ref, std::ref(x));    // ref to x
    t2.join();
    std::cout << "after t2 x=" << x << "\n";

    std::thread t3(many, "hello", 7, 3.14); // multiple args, perfect-forwarded
    t3.join();

    // Lambda with capture (often clearer than std::ref):
    int y = 0;
    std::thread t4([&y]{ y = 99; });
    t4.join();
    std::cout << "after t4 y=" << y << "\n";
    return 0;
}
