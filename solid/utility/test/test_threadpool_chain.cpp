#include "solid/system/crashhandler.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/threadpool.hpp"
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

int test_threadpool_chain(int argc, char* argv[])
{
    install_crash_handler();
    solid::log_start(std::cerr, {".*:EWXS", "test_basic:VIEWS"});
    using ThreadPoolT = ThreadPool<size_t, size_t>;
    using AtomicPWPT  = std::atomic<ThreadPoolT*>;

    solid_log(logger, Statistic, "thread concurrency: " << thread::hardware_concurrency());

    const int           wait_seconds = 300;
    int                 loop_cnt     = 2;
    const size_t        cnt{1000000};
    const size_t        v = (((cnt - 1) * cnt)) / 2;
    std::atomic<size_t> val{0};
    AtomicPWPT          pwp{nullptr};
    size_t              thread_count = 0;

    if (argc > 1) {
        thread_count = atoi(argv[1]);
    }

    if (argc > 2) {
        loop_cnt = atoi(argv[2]);
    }

    if (thread_count == 0) {
        thread_count = thread::hardware_concurrency();
    }

    auto lambda = [&]() {
        for (int i = 0; i < loop_cnt; ++i) {
            {
                ThreadPoolT wp_b{
                    {thread_count, 10000, 1000}, [](const size_t) {}, [](const size_t) {},
                    [&val](const size_t _v) {
                        val += _v;
                    },
                    [](const size_t) {}};
                ThreadPoolT wp_f{
                    {thread_count, 10000, 1000}, [](const size_t) {}, [](const size_t) {},
                    [&wp_b](const size_t _v) {
                        wp_b.pushOne(_v);
                    },
                    [](const size_t) {}};

                pwp = &wp_b;

                for (size_t i = 0; i < cnt; ++i) {
                    wp_f.pushOne(i);
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
            solid_log(logger, Statistic, "ThreadPool statistic: " << pwp.load()->statistic());
        }
        solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
    }
    fut.get();
    solid_log(logger, Verbose, "after async wait");

    return 0;
}
