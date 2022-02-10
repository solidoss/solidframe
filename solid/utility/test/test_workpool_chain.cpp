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

int test_workpool_chain(int argc, char* argv[])
{
    install_crash_handler();
    solid::log_start(std::cerr, {".*:EWXS", "test_basic:VIEWS"});
#if SOLID_WORKPOOL_OPTION == 0
    using WorkPoolT = lockfree::WorkPool<size_t, void, workpool_default_node_capacity_bit_count, impl::StressTestWorkPoolBase<100>>;
#elif SOLID_WORKPOOL_OPTION == 1
    using WorkPoolT = locking::WorkPool<size_t, void, workpool_default_node_capacity_bit_count, impl::StressTestWorkPoolBase<100>>;
#else
    using WorkPoolT = locking::WorkPool<size_t, size_t, workpool_default_node_capacity_bit_count, impl::StressTestWorkPoolBase<100>>;
#endif

    using AtomicPWPT = std::atomic<WorkPoolT*>;

    solid_log(logger, Statistic, "thread concurrency: " << thread::hardware_concurrency());

    const int           wait_seconds = 300;
    int                 loop_cnt     = 2;
    size_t              start_thr    = 0;
    const size_t        cnt{1000000};
    const size_t        v = (((cnt - 1) * cnt)) / 2;
    std::atomic<size_t> val{0};
    AtomicPWPT          pwp{nullptr};
    size_t              thread_count = 0;

    if (argc > 1) {
        thread_count = atoi(argv[1]);
    }

    if (argc > 2) {
        start_thr = atoi(argv[2]);
    }

    if (argc > 3) {
        loop_cnt = atoi(argv[3]);
    }

    if (thread_count == 0) {
        thread_count = thread::hardware_concurrency();
    }
    if (start_thr > thread_count) {
        start_thr = thread_count;
    }

    auto lambda = [&]() {
        for (int i = 0; i < loop_cnt; ++i) {
            {
                WorkPoolT wp_b
                {
                    WorkPoolConfiguration(thread_count),
                        [&val](const size_t _v) {
                            val += _v;
                        }
#if SOLID_WORKPOOL_OPTION == 2
                    ,
                        [](const size_t) {}
#endif
                };
                WorkPoolT wp_f
                {
                    WorkPoolConfiguration(thread_count),
                        [&wp_b](const size_t _v) {
                            wp_b.push(_v);
                        }
#if SOLID_WORKPOOL_OPTION == 2
                    ,
                        [](const size_t) {}
#endif
                };

                pwp = &wp_b;

                for (size_t i = 0; i < cnt; ++i) {
                    wp_f.push(i);
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
