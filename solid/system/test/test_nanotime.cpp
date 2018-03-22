#include "solid/system/convertors.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/nanotime.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <time.h>

using namespace solid;
using namespace std;

int test_nanotime(int /*argc*/, char**const /*argv*/)
{
    for (int i = 0; i < 1; ++i) {
        auto steady_now = std::chrono::steady_clock::now();

        NanoTime nano_now{steady_now};

        auto now2 = nano_now.timePointCast<std::chrono::steady_clock::time_point>();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        //nano_now -> system clock timepoint
        auto now3 = nano_now.timePointClockCast<std::chrono::system_clock::time_point, std::chrono::steady_clock>();

        NanoTime system_now{now3};

        cout << nano_now.tv_sec << ":" << nano_now.tv_nsec << endl;
        cout << system_now.tv_sec << ":" << system_now.tv_nsec << endl;

        auto now4 = system_now.timePointClockCast<std::chrono::steady_clock::time_point, std::chrono::system_clock>();
        {
            NanoTime n_now{now4};
            cout << n_now.tv_sec << ":" << n_now.tv_nsec << endl;
        }
        auto now5 = system_now.timePointClockCast<std::chrono::system_clock::time_point, std::chrono::system_clock>();
        {
            NanoTime n_now{now5};
            cout << n_now.tv_sec << ":" << n_now.tv_nsec << endl;
        }
        cout << endl;

        SOLID_CHECK(steady_now == now2);
        //SOLID_CHECK(steady_now == now4);
    }
    return 0;
}
