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

struct Context {
    std::atomic<uint32_t> all_val{0};
    std::atomic<size_t>   val{0};
};

const size_t worker_count = 2;

} // namespace

int test_callpool_multicast_basic(int argc, char* argv[])
{
    install_crash_handler();

    solid::log_start(std::cerr, {".*:EWXS", "test:VIEWS"});
    using CallPoolT  = ThreadPool<Function<void(Context&, deque<uint32_t>&)>, Function<void(Context&, deque<uint32_t>&)>>;
    using AtomicPWPT = std::atomic<CallPoolT*>;

    solid_log(logger, Statistic, "thread concurrency: " << thread::hardware_concurrency());
#ifdef SOLID_SANITIZE_THREAD
    const int wait_seconds = 1500;
#else
    const int wait_seconds = 150;
#endif
    int          loop_cnt = 5;
    const size_t cnt{5000000};
    const size_t v = (((cnt - 1) * cnt)) / 2;
    Context      context;
    AtomicPWPT   pwp{nullptr};

    if (argc > 1) {
        loop_cnt = atoi(argv[1]);
    }
    auto lambda = [&]() {
        for (int i = 0; i < loop_cnt; ++i) {
            deque<uint32_t> record_dq;
            record_dq.resize(cnt, -1);
            atomic<size_t> worker_start_count{0};
            atomic<size_t> worker_stop_count{0};
            auto           worker_start = [&worker_start_count](const size_t, Context&, deque<uint32_t>&) {
                ++worker_start_count;
                thread_local_value = 0;
            };
            auto worker_stop = [&worker_stop_count](const size_t, Context&, deque<uint32_t>&) {
                ++worker_stop_count;
            };
            {
                CallPoolT cp{{worker_count, 100000, 1000}, worker_start, worker_stop, std::ref(context), std::ref(record_dq)};

                pwp = &cp;
                std::promise<void> barrier;
                std::future<void>  barrier_future = barrier.get_future();

                std::thread all_thr{
                    [&cp, cnt](std::promise<void> barrier) {
                        barrier.set_value();
                        for (uint32_t i = 0; i < cnt; ++i) {
                            if ((i % 10) == 0) {
                                cp.pushAll(
                                    [v = (i / 10 + 1)](Context& _rctx, deque<uint32_t>&) {
                                        uint32_t expect = thread_local_value;
                                        _rctx.all_val.compare_exchange_strong(expect, v);
                                        solid_check_log(thread_local_value < v, logger, "" << thread_local_value << " < " << v);
                                        thread_local_value = v;
                                    });
                            }
                        }
                    },
                    std::move(barrier)};

                barrier_future.wait();

                for (size_t i = 0; i < cnt; ++i) {
                    cp.pushOne(
                        [i](Context& _rctx, deque<uint32_t>& _rdq) {
                            _rctx.val += i;
                            solid_check_log(_rdq[i] == static_cast<uint32_t>(-1), logger, "push one check" << i);
                            _rdq[i] = thread_local_value;
                        });
                };

                pwp = nullptr;

                all_thr.join();
                cp.stop();
                solid_log(logger, Statistic, "CP stats" << cp.statistic());
            }

            solid_log(logger, Verbose, "after loop");

            solid_check_log(worker_start_count == worker_count && worker_stop_count == worker_count, logger, "first check after loop");

            for (size_t i = 1; i < record_dq.size(); ++i) {
                solid_check_log(record_dq[i] >= record_dq[i - 1], logger, "record check " << i << ' ' << record_dq[i] << " vs " << record_dq[i - 1]);
            }

            solid_check_log(v == context.val, logger, "val = " << context.val << " v = " << v);
            context.val = 0;
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
