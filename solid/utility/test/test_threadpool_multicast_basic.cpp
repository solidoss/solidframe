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
const LoggerT logger("test");

thread_local uint32_t thread_local_value = 0;

} // namespace

int test_threadpool_multicast_basic(int argc, char* argv[])
{
    install_crash_handler();
    solid::log_start(std::cerr, {".*:EWXS", "test:VIEWS"});
    using ThreadPoolT = ThreadPool<size_t, size_t>;
    using AtomicPWPT  = std::atomic<ThreadPoolT*>;

    solid_log(logger, Statistic, "thread concurrency: " << thread::hardware_concurrency());
#ifdef SOLID_SANITIZE_THREAD
    const int wait_seconds = 1500;
#else
    const int wait_seconds = 150;
#endif
    int                   loop_cnt = 5;
    const size_t          cnt{5000000};
    const size_t          v = (((cnt - 1) * cnt)) / 2;
    std::atomic<uint32_t> all_val{0};
    std::atomic<size_t>   val{0};
    AtomicPWPT            pwp{nullptr};

    if (argc > 1) {
        loop_cnt = atoi(argv[1]);
    }
    auto lambda = [&]() {
        for (int i = 0; i < loop_cnt; ++i) {
            deque<uint32_t> record_dq;
            record_dq.resize(cnt, -1);
            {
                ThreadPoolT wp{
                    {2, 10000, 1000}, [](const size_t) {}, [](const size_t) {},
                    [&val, &record_dq](const size_t _v) {
                        val += _v;
                        solid_check(record_dq[_v] == static_cast<uint32_t>(-1));
                        record_dq[_v] = thread_local_value;
                        // solid_log(logger, Verbose, "job "<<_v);
                    },
                    [&all_val](const size_t _v) { // mcast execute
                        const uint32_t value  = static_cast<uint32_t>(_v);
                        uint32_t       expect = thread_local_value;
                        all_val.compare_exchange_strong(expect, value);

                        thread_local_value = value;
                        // solid_log(logger, Verbose, "mcast");
                    }};

                pwp = &wp;
                std::promise<void> barrier;
                std::future<void>  barrier_future = barrier.get_future();

                std::thread all_thr{
                    [&wp, cnt](std::promise<void> barrier) {
                        barrier.set_value();
                        for (uint32_t i = 0; i < cnt; ++i) {
                            if ((i % 10) == 0) {
                                wp.pushAll(i / 10 + 1);
                            }
                        }
                    },
                    std::move(barrier)};

                barrier_future.wait();

                for (size_t i = 0; i < cnt; ++i) {
                    wp.pushOne(i);
                };

                pwp = nullptr;

                all_thr.join();
            }

            solid_log(logger, Verbose, "after loop");

            for (size_t i = 1; i < record_dq.size(); ++i) {
                solid_check(record_dq[i] >= record_dq[i - 1]);
            }

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
