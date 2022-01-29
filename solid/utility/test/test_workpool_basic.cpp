#include "solid/system/crashhandler.hpp"
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
namespace {
const LoggerT logger("test_basic");
}

int test_workpool_basic(int argc, char* argv[])
{
    install_crash_handler();
    solid::log_start(std::cerr, {".*:EWXS", "test_basic:VIEWS"});
#if SOLID_WORKPOOL_OPTION == 0
    using WorkPoolT = lockfree::WorkPool<size_t>;
#elif SOLID_WORKPOOL_OPTION == 1
    using WorkPoolT        = locking::WorkPool<size_t>;
#else
    using WorkPoolT = locking::WorkPool<size_t, size_t>;
#endif

    using AtomicPWPT = std::atomic<WorkPoolT*>;

    solid_log(logger, Statistic, "thread concurrency: " << thread::hardware_concurrency());
#ifdef SOLID_SANITIZE_THREAD
    const int wait_seconds = 1500;
#else
    const int wait_seconds = 300;
#endif
    int                 loop_cnt = 3;
    const size_t        cnt{5000000};
    const size_t        v = (((cnt - 1) * cnt)) / 2;
    std::atomic<size_t> val{0};
    AtomicPWPT          pwp{nullptr};

    if (argc > 1) {
        loop_cnt = atoi(argv[1]);
    }
    auto lambda = [&]() {
        for (int i = 0; i < loop_cnt; ++i) {
            {
                WorkPoolT wp
                {
                    WorkPoolConfiguration(),
#if SOLID_WORKPOOL_OPTION < 2
                        2,
#endif
                        [&val](const size_t _v) {
                            val += _v;
                        }
#if SOLID_WORKPOOL_OPTION == 2
                    ,
                        [](const size_t) {}
#endif
                };
                pwp = &wp;
                for (size_t i = 0; i < cnt; ++i) {
                    wp.push(i);
                };
                pwp = nullptr;
            }
            solid_log(logger, Verbose, "after loop");
            solid_check(v == val, "val = " << val << " v = " << v);
            val = 0;
        }
    };
    auto fut = async(launch::async, lambda);
    if (fut.wait_for(chrono::seconds(wait_seconds)) != future_status::ready) {
        if (pwp != nullptr) {
            solid_log(logger, Statistic, "Workpool statistic: " << pwp.load()->statistic());
        }
        solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
    }
    fut.get();
    solid_log(logger, Verbose, "after async wait");

    return 0;
}
