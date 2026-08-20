// 01_jthread.cpp -- C++20 std::jthread + std::stop_token.
//
//  Two things every C++20 program should adopt:
//
//   1. std::jthread auto-joins in its destructor (no more "forgot to join").
//   2. std::stop_token gives cooperative, safe cancellation.
//
//      std::jthread t([](std::stop_token st){
//          while (!st.stop_requested()) { ... }
//      });
//      t.request_stop();   // sets the token
//      // destructor join()s
//
//  Picture:
//
//      main                            jthread worker
//      ----                            --------------
//      jthread t(fn);                  fn(stop_token) {
//      ...                                while (!st.stop_requested())
//      // t goes out of scope:                do_work();
//      //   t.request_stop();              cleanup;
//      //   t.join();                    }
//
//  Compile: g++ -std=c++20 -pthread -O2 -o 01_jthread 01_jthread.cpp
//
//  Falls back to a friendly message if compiled without C++20 jthread support.

#include <version>     // brings __cpp_lib_jthread macro
#include <iostream>

#if __cpp_lib_jthread >= 201911L

#include <thread>
#include <stop_token>
#include <chrono>

using namespace std::chrono_literals;

int main()
{
    std::jthread t([](std::stop_token st){
        int n = 0;
        while (!st.stop_requested()) {
            std::cout << "  worker tick " << ++n << "\n";
            std::this_thread::sleep_for(150ms);
        }
        std::cout << "  worker exiting cleanly\n";
    });

    std::this_thread::sleep_for(800ms);
    std::cout << "[main] requesting stop\n";
    t.request_stop();          // jthread destructor will also auto-join
}

#else

int main() {
    std::cout << "Compile with -std=c++20 and a recent libc++/libstdc++ for jthread.\n";
}

#endif
