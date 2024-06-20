#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/threadpool.hpp"
#include <atomic>
#include <future>
#include <iostream>

using namespace solid;
using namespace std;
namespace {
const LoggerT logger("test_context");

struct Context {
    string         s_;
    atomic<size_t> v_;
    const size_t   c_;

    Context(const string& _s, const size_t _v, const size_t _c)
        : s_(_s)
        , v_(_v)
        , c_(_c)
    {
    }

    ~Context()
    {
    }
};
using CallPoolT = ThreadPool<Function<void(Context&)>, Function<void(Context&)>>;
} // namespace

int test_threadpool_context(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:EWXS", "test_context:VIEWS"});

    int          wait_seconds = 500;
    int          loop_cnt     = 5;
    const size_t cnt{50000};

    auto lambda = [&]() {
#if 1
        for (int i = 0; i < loop_cnt; ++i) {
            Context ctx{"test", 1, cnt + 1};
            {
                CallPoolT wp{
                    {2, 10000, 100}, [](const size_t, Context&) {}, [](const size_t, Context& _rctx) {},
                    std::ref(ctx)};

                solid_log(generic_logger, Verbose, "wp started");
                for (size_t i = 0; i < cnt; ++i) {
                    auto l = [](Context& _rctx) {
                        ++_rctx.v_;
                    };
                    wp.pushOne(l);
                };
            }
            solid_check(ctx.v_ == ctx.c_, ctx.v_ << " != " << ctx.c_);
            solid_log(logger, Verbose, "after loop");
        }
#endif
    };

    auto fut = async(launch::async, lambda);
    if (fut.wait_for(chrono::seconds(wait_seconds)) != future_status::ready) {
        solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
    }
    fut.get();
    solid_log(logger, Verbose, "after async wait");

    return 0;
}
