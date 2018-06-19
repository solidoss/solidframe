#include "solid/system/exception.hpp"
#include "solid/utility/workpool.hpp"
#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

using namespace solid;
using namespace std;

int test_workpool_basic(int /*argc*/, char* /*argv*/ [])
{
    solid::log_start(std::cerr, {".*:EWS"});

    const int           wait_seconds = 100;
    const size_t        cnt{5000000};
    std::atomic<size_t> val{0};
    promise<void>       prom;

    thread wait_thread(
        [](promise<void>& _rprom, const int _wait_time_seconds) {
            SOLID_CHECK(_rprom.get_future().wait_for(chrono::seconds(_wait_time_seconds)) == future_status::ready, " Test is taking too long - waited " << _wait_time_seconds << " secs");
        },
        std::ref(prom), wait_seconds);

    {
        WorkPool<size_t> wp{
            0,
            WorkPoolConfiguration(),
            [&val](size_t _v) {
                val += _v;
            }};

        for (size_t i = 0; i < cnt; ++i) {
            wp.push(i);
        };
    }
    prom.set_value();
    wait_thread.join();
    solid_log(generic_logger, Verbose, "val = " << val);

    const size_t v = ((cnt - 1) * cnt) / 2;

    SOLID_CHECK(v == val);
    return 0;
}
