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

using FunctionJobT = std::function<void(Context&)>;
//using FunctionJobT = solid::Function<32, void(Context&)>;

} // namespace

int test_workpool_context(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:EWS", "test_context:VIEW"});
    using WorkPoolT  = WorkPool<FunctionJobT>;
    using AtomicPWPT = std::atomic<WorkPoolT*>;

    int                 wait_seconds = 500;
    int                 loop_cnt     = 5;
    const size_t        cnt{5000000};
    std::atomic<size_t> val{0};
    promise<void>       prom;
    AtomicPWPT          pwp{nullptr};
    const size_t        v = ((cnt - 1) * cnt) / 2;
    thread              wait_thread(
        [](promise<void>& _rprom, AtomicPWPT& _rpwp, const int _wait_time_seconds) {
            if (_rprom.get_future().wait_for(chrono::seconds(_wait_time_seconds)) != future_status::ready) {
                if (_rpwp != nullptr) {
                    _rpwp.load()->dumpStatistics();
                }
                solid_throw(" Test is taking too long - waited " << _wait_time_seconds << " secs");
            }
        },
        std::ref(prom), std::ref(pwp), wait_seconds);

    if (argc > 1) {
        loop_cnt = atoi(argv[1]);
    }
    for (int i = 0; i < loop_cnt; ++i) {
        {
            WorkPoolT wp{
                WorkPoolConfiguration(),
                [](FunctionJobT& _rj, Context& _rctx) {
                    _rj(_rctx);
                },
                "simple text",
                0UL};

            solid_log(generic_logger, Verbose, "wp started");
            pwp = &wp;
            for (size_t i = 0; i < cnt; ++i) {
                auto l = [i, &val](Context& _rctx) {
                    //this_thread::sleep_for(std::chrono::seconds(2));
                    ++_rctx.count_;
                    val += i;
                };
                wp.push(l);
            };
            pwp = nullptr;
        }
        solid_log(logger, Verbose, "after loop");
        solid_check(v == val, "val = " << val << " v = " << v);
        val = 0;
    }

    prom.set_value();
    solid_log(logger, Verbose, "after promise set value - before join");
    wait_thread.join();
    solid_log(logger, Verbose, "after join");
    return 0;
}
