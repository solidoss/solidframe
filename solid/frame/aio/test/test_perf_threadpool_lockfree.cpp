#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

#include "solid/system/crashhandler.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/event.hpp"
#include "solid/utility/threadpool.hpp"

using namespace solid;
using namespace std;
namespace {
const LoggerT logger("test");

using ThreadPoolT = ThreadPool<Event<128>, size_t>;
atomic<size_t> received_events{0};
atomic<size_t> accumulate_value{0};

} // namespace

int test_perf_threadpool_lockfree(int argc, char* argv[])
{

    solid::log_start(std::cerr, {".*:EWXS", "test:VIEWS"});

#ifdef SOLID_SANITIZE_THREAD
    const int wait_seconds = 1500;
#else
    const int wait_seconds = 10000;
#endif

    size_t event_count{1000000};
    size_t thread_count{4};
    size_t context_count{4};

    if (argc > 1) {
        thread_count = stoul(argv[1]);
    }
    if (argc > 2) {
        event_count = stoul(argv[2]);
    }
    if (argc > 3) {
        context_count = stoul(argv[3]);
    }
    (void)context_count;
    auto lambda = [&]() {
        ThreadPoolT wp{
            {thread_count, 10000, 0}, [](const size_t) {}, [](const size_t) {},
            [&](EventBase& _event) {
                if (_event == generic_event<GenericEventE::Wake>) {
                    ++received_events;
                    accumulate_value += *_event.cast<size_t>();
                }
                // solid_log(logger, Verbose, "job " << _r.value_);
            },
            [](const size_t) {}};
        for (size_t i = 0; i < event_count; ++i) {
            wp.pushOne(make_event(GenericEventE::Wake, i));
        }
    };

    auto fut = async(launch::async, lambda);
    if (fut.wait_for(chrono::seconds(wait_seconds)) != future_status::ready) {

        solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
    }
    fut.get();

    solid_log(logger, Verbose, "after async wait " << received_events << " " << accumulate_value);

    return 0;
}
