#include "solid/system/crashhandler.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/threadpool.hpp"
#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <set>
#include <thread>

using namespace solid;
using namespace std;
namespace {
const LoggerT logger("test");

struct Context {
    std::atomic<uint32_t> all_val{0};
    std::atomic<size_t>   val{0};
};

thread_local set<size_t> local_set;

} // namespace

int test_callpool_multicast_pattern(int argc, char* argv[])
{
    install_crash_handler();

    solid::log_start(std::cerr, {".*:EWXS", "test:VIEWS"});
    using CallPoolT  = ThreadPool<Function<void(Context&)>, Function<void(Context&)>>;
    using AtomicPWPT = std::atomic<CallPoolT*>;

    solid_log(logger, Statistic, "thread concurrency: " << thread::hardware_concurrency());
#ifdef SOLID_SANITIZE_THREAD
    const int wait_seconds = 1500;
#else
    const int wait_seconds = 150;
#endif
    int          loop_cnt = 5;
    const size_t cnt{500000};
    Context      context;
    AtomicPWPT   pwp{nullptr};

    if (argc > 1) {
        loop_cnt = atoi(argv[1]);
    }
    auto lambda = [&]() {
        for (int i = 0; i < loop_cnt; ++i) {
            {
                CallPoolT cp{{2, 100000, 100000}, [](const size_t, Context&) {}, [](const size_t, Context& _rctx) {}, std::ref(context)};
                pwp = &cp;

                for (size_t j = 0; j < cnt; ++j) {
                    cp.pushAll([j](Context&) {
                        local_set.insert(j);
                        // solid_log(logger, Info, "local insert "<<j);
                    });
                    cp.pushOne([j](Context&) {
                        solid_check(local_set.find(j) != local_set.end());
                        // solid_log(logger, Info, "local check "<<j);
                    });
                    cp.pushAll([j](Context&) {
                        local_set.erase(j);
                        // solid_log(logger, Info, "local eraser "<<j);
                    });
                    cp.pushOne([j](Context&) {
                        solid_check(local_set.find(j) == local_set.end());
                        // solid_log(logger, Info, "local check "<<j);
                    });
                }
            }

            solid_log(logger, Verbose, "after loop");

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
