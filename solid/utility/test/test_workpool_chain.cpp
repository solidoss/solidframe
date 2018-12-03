#include "solid/system/exception.hpp"
#include "solid/system/crashhandler.hpp"
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
    solid::log_start(std::cerr, {".*:EWS", "test_basic:VIEWS"});
    using WorkPoolT  = WorkPool<size_t>;
    using AtomicPWPT = std::atomic<WorkPoolT*>;

    solid_log(logger, Statistic, "thread concurrency: " << thread::hardware_concurrency());

    const int           wait_seconds = 500;
    int                 loop_cnt     = 2;
    size_t              start_thr    = 0;
    const size_t        cnt{5000000};
    const size_t        v = (((cnt - 1) * cnt)) / 2;
    std::atomic<size_t> val{0};
    AtomicPWPT          pwp{nullptr};

    if (argc > 1) {
        start_thr = atoi(argv[1]);
    }

    if (argc > 2) {
        loop_cnt = atoi(argv[2]);
    }
    auto lambda = [&]() {
        for (int i = 0; i < loop_cnt; ++i) {
            {
                WorkPoolT wp_b{
                    start_thr,
                    WorkPoolConfiguration(),
                    [&val](const size_t _v) {
                        val += _v;
                    }};
                WorkPoolT wp_f{
                    start_thr,
                    WorkPoolConfiguration(),
                    [&wp_b](const size_t _v) {
                        wp_b.push(_v);
                    }};

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
    if(async(launch::async, lambda).wait_for(chrono::seconds(wait_seconds)) != future_status::ready){
        if (pwp != nullptr) {
            pwp.load()->dumpStatistics();
        }
        solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
    }
    solid_log(logger, Verbose, "after async wait");

    return 0;
}
