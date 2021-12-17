#include "solid/system/crashhandler.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/workpool.hpp"
#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

using namespace solid;
using namespace std;

using thread_t      = std::thread;
using mutex_t       = std::mutex;
using unique_lock_t = std::unique_lock<mutex_t>;

namespace {

mutex_t       mtx;
const LoggerT logger("test_context");

struct Context {
    string text_;
    size_t count_;

    Context(const std::string& _txt, size_t _count)
        : text_(_txt)
        , count_(_count)
    {
    }
    ~Context()
    {
        solid_log(generic_logger, Verbose, this << " count = " << count_);
    }
};
template <class Job, class MCast>
#if SOLID_WORKPOOL_OPTION == 0
using WorkPoolT = lockfree::WorkPool<Job, void, workpool_default_node_capacity_bit_count, impl::StressTestWorkPoolBase<90>>;
#elif SOLID_WORKPOOL_OPTION == 1
using WorkPoolT = locking::WorkPool<Job, void, workpool_default_node_capacity_bit_count, impl::StressTestWorkPoolBase<90>>;
#else
using WorkPoolT = locking::WorkPool<Job, MCast, workpool_default_node_capacity_bit_count, impl::StressTestWorkPoolBase<90>>;
#endif

} // namespace

int test_workpool_thread_context(int argc, char* argv[])
{
    install_crash_handler();
    solid::log_start(std::cerr, {".*:EWXS", "test_context:VIEWS"});
#if SOLID_WORKPOOL_OPTION == 0
    using CallPoolT = lockfree::CallPoolT<void(Context&), void, WorkPoolT>;
#elif SOLID_WORKPOOL_OPTION == 1
    using CallPoolT  = locking::CallPoolT<void(Context&), void, WorkPoolT>;
#else
    using CallPoolT = locking::CallPoolT<void(Context&), void(Context&), WorkPoolT>;
#endif

    using AtomicPWPT = std::atomic<CallPoolT*>;

    solid_log(logger, Statistic, "thread concurrency: " << thread::hardware_concurrency());

    solid_log(logger, Warning, "alignof(std::function) = " << alignof(std::function<void(Context&)>));
#ifdef SOLID_SANITIZE_THREAD
    int wait_seconds = 1000;
#else
    int wait_seconds = 200;
#endif
    int                 loop_cnt = 3;
    const size_t        cnt{5000000};
    std::atomic<size_t> val{0};
    AtomicPWPT          pwp{nullptr};
    const size_t        v = ((cnt - 1) * cnt) / 2;

    if (argc > 1) {
        loop_cnt = atoi(argv[1]);
    }

    auto lambda = [&]() {
        for (int i = 0; i < loop_cnt; ++i) {
            auto start = chrono::steady_clock::now();
            {
                CallPoolT wp
                {
                    WorkPoolConfiguration(2),
#if SOLID_WORKPOOL_OPTION < 2
                        0,
#endif
                        Context("simple text", 0UL)
                };

                solid_log(logger, Verbose, "wp started");
                pwp   = &wp;
                start = chrono::steady_clock::now();
                for (size_t i = 0; i < cnt; ++i) {
                    auto l = [i, &val](Context& _rctx) {
                        ++_rctx.count_;
                        val += i;
                    };
                    wp.push(l);
                };

                pwp = nullptr;
                solid_log(logger, Verbose, "completed all pushes - wating for workpool to terminate: " << chrono::duration<double>(chrono::steady_clock::now() - start).count());
            }
            solid_log(logger, Verbose, "after done: " << chrono::duration<double>(chrono::steady_clock::now() - start).count());
            solid_check(v == val, "val = " << val << " v = " << v);
            val = 0;
        }
    };

    auto fut = async(launch::async, lambda);
    if (fut.wait_for(chrono::seconds(wait_seconds)) != future_status::ready) {
        if (pwp != nullptr) {
            solid_log(logger, Statistic, "Workpool statistic: " << pwp.load()->statistic());
        }
        solid_log(logger, Error, "Waited too much. Wait some more for workpool internal checkpoints to fire...");
        this_thread::sleep_for(chrono::seconds(100));

        solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
    }
    fut.get();
    solid_log(logger, Verbose, "after async wait");

    return 0;
}
