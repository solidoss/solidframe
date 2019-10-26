#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/workpool.hpp"
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

} //namespace

int test_workpool_context(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:EWS", "test_context:VIEWS"});

    int          wait_seconds = 500;
    int          loop_cnt     = 5;
    const size_t cnt{50000};

    auto lambda = [&]() {
#if 1
        for (int i = 0; i < loop_cnt; ++i) {
            Context ctx{"test", 1, cnt + 1};
            {
                CallPool<void(Context&)> wp{
                    WorkPoolConfiguration(), 2,
                    std::ref(ctx)};

                solid_log(generic_logger, Verbose, "wp started");
                for (size_t i = 0; i < cnt; ++i) {
                    auto l = [](Context& _rctx) {
                        ++_rctx.v_;
                    };
                    wp.push(l);
                };
            }
            solid_check(ctx.v_ == ctx.c_, ctx.v_ << " != " << ctx.c_);
            solid_log(logger, Verbose, "after loop");
        }
#endif
    };
    if (async(launch::async, lambda).wait_for(chrono::seconds(wait_seconds)) != future_status::ready) {
        solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
    }
    solid_log(logger, Verbose, "after async wait");

    return 0;
}
