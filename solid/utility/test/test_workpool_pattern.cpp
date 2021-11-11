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
const LoggerT logger("test_pattern");
}

int test_workpool_pattern(int argc, char* argv[])
{
    install_crash_handler();
    solid::log_start(std::cerr, {".*:EWXS", "test_basic:VIEWS"});

    using WorkPoolT  = WorkPool<size_t>;
    using AtomicPWPT = std::atomic<WorkPoolT*>;

    solid_log(logger, Statistic, "thread concurrency: " << thread::hardware_concurrency());

    const int                          wait_seconds = 500;
    const size_t                       producer_cnt = 2;
    const size_t                       consumer_cnt = 2;
    const uint64_t                     check_sum    = 10999970000000UL;
    std::atomic<uint64_t>              sum{0};
    AtomicPWPT                         pwp{nullptr};
    const vector<pair<size_t, size_t>> producer_pattern = {
        {100000, 10},
        {200000, 20},
        {300000, 30},
        {400000, 40},
        {500000, 50},
        {500000, 50},
        {400000, 40},
        {300000, 30},
        {200000, 20},
        {100000, 10},
    };
    const vector<pair<size_t, size_t>> consummer_pattern = {
        {500000, 50},
        {400000, 40},
        {300000, 30},
        {200000, 20},
        {100000, 10},
        {100000, 10},
        {200000, 20},
        {300000, 30},
        {400000, 40},
        {500000, 50},
    };
    const size_t producer_loop_count{producer_pattern.size() * 10};

    auto lambda = [&]() {
        WorkPoolT wp{
            WorkPoolConfiguration(consumer_cnt),
            consumer_cnt,
            [&sum, &consummer_pattern, loop = consummer_pattern[0].first, idx = 0](const size_t _v) mutable {
                sum += _v;
                --loop;
                if (loop == 0) {
                    this_thread::sleep_for(chrono::milliseconds(consummer_pattern[idx % consummer_pattern.size()].second));
                    ++idx;
                    loop = consummer_pattern[idx % consummer_pattern.size()].first;
                }
            }};

        vector<thread> thr_vec;

        for (size_t i = 0; i < producer_cnt; ++i) {
            thr_vec.emplace_back(
                thread{
                    [producer_loop_count, &producer_pattern](WorkPoolT& _wp) {
                        for (size_t i = 0; i < producer_loop_count; ++i) {
                            const size_t cnt = producer_pattern[i % producer_pattern.size()].first;
                            for (size_t j = 0; j < cnt; ++j) {
                                _wp.push(j);
                            }
                            this_thread::sleep_for(chrono::milliseconds(producer_pattern[i % producer_pattern.size()].second));
                        }
                    },
                    ref(wp)});
        }

        for (auto& thr : thr_vec) {
            thr.join();
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
    solid_check(sum == check_sum, "sum = " << sum << " check_sum = " << check_sum);

    solid_log(logger, Verbose, "after async wait");

    return 0;
}
