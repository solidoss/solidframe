#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/threadpool.hpp"

using namespace std;
using namespace solid;

namespace {

#ifdef SOLID_ON_LINUX
vector<int> isolcpus = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15, 17, 18, 19};

void set_current_thread_affinity()
{
    static std::atomic<int> crtCore(0);

    const int isolCore = isolcpus[crtCore.fetch_add(1) % isolcpus.size()];
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(isolCore, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    // solid_check(rc == 0);
    (void)rc;
}
#else
void set_current_thread_affinity()
{
}
#endif

const LoggerT logger("test");

struct Context {
    std::atomic_size_t sum_{0};
};

using CallPoolT = ThreadPool<
    SmallFunction64T<void(Context&)>,
    SmallFunction64T<void(Context&)>,
    DefaultThreadPoolTraits<EmptyThreadPoolStatistic>>;

size_t thread_local local_sum        = 0;
size_t thread_local local_call_count = 0;

} // namespace

int test_threadpool_perf_basic(int argc, char* argv[])
{
    solid::log_start(std::cerr, {"test:VIEWXS"});

    size_t thread_count = 4;
    size_t push_count   = 100000000;

    if (argc > 1) {
        push_count = stoul(argv[1]);
    }

    if (argc > 2) {
        thread_count = stoul(argv[2]);
    }

    Context   ctx;
    CallPoolT tp{{thread_count, 1000 * 10, 100},
        [](const size_t, Context&) {
            set_current_thread_affinity();
        },
        [](const size_t _idx, Context& _rctx) {
            _rctx.sum_ += local_sum;
            solid_log(logger, Info, "local_call_count(" << _idx << "): " << local_call_count);
        },
        std::ref(ctx)};

    for (size_t i = 0; i < push_count; ++i) {
        tp.pushOne([i](Context&) {
            local_sum += i;
            ++local_call_count;
        });
    }
    return 0;
}