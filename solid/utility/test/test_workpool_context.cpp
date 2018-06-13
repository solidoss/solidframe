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

mutex_t mtx;

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

int test_workpool_context(int /*argc*/, char* /*argv*/ [])
{
    solid::log_start(std::cerr, {".*:EWS"});

    int                 wait_seconds = 10;
    const size_t        cnt{5000000};
    std::atomic<size_t> val{0};
    promise<void>       prom;

    thread wait_thread(
        [](promise<void>& _rprom, const int _wait_time_seconds) {
            SOLID_CHECK(_rprom.get_future().wait_for(chrono::seconds(_wait_time_seconds)) == future_status::ready, " Test is taking too long - waited " << _wait_time_seconds << " secs");
        },
        std::ref(prom), wait_seconds);

    {
        WorkPool<FunctionJobT> wp{
            WorkPoolConfiguration(),
            [](FunctionJobT& _rj, Context& _rctx) {
                _rj(_rctx);
            },
            "simple text",
            0UL};

        solid_log(generic_logger, Verbose, "wp started");

        for (size_t i = 0; i < cnt; ++i) {
            auto l = [i, &val](Context& _rctx) {
                //this_thread::sleep_for(std::chrono::seconds(2));
                ++_rctx.count_;
                val += i;
            };
            wp.push(l);
        };
    }

    prom.set_value();
    wait_thread.join();

    solid_log(generic_logger, Verbose, "val = " << val);

    const size_t v = ((cnt - 1) * cnt) / 2;

    SOLID_CHECK(v == val);
    return 0;
}
