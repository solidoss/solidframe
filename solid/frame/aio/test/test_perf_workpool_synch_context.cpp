#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

#include "solid/system/crashhandler.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/event.hpp"
#include "solid/utility/workpool.hpp"

using namespace solid;
using namespace std;
namespace {
const LoggerT logger("test");

using WorkPoolT     = locking::WorkPoolT<Event, uint32_t>;
using SynchContextT = WorkPoolT::SynchronizationContextT;

atomic<size_t> received_events{0};
atomic<size_t> accumulate_value{0};

} // namespace

int test_perf_workpool_synch_context(int argc, char* argv[])
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

    auto lambda = [&]() {
        auto      start = std::chrono::steady_clock::now();
        WorkPoolT wp{
            WorkPoolConfiguration{thread_count},
            [&](Event& _event) {
                if (_event == generic_event_raise) {
                    ++received_events;
                    accumulate_value += *_event.any().cast<size_t>();
                }
                // solid_log(logger, Verbose, "job " << _r.value_);
            },
            [](const uint32_t _v) { // mcast execute
                // solid_log(logger, Verbose, "mcast " << _v);
            }

        };
        {
            vector<SynchContextT> synch_contexts(context_count);

            for (size_t i = 0; i < context_count; ++i) {
                synch_contexts[i] = wp.createSynchronizationContext();
            }

            for (size_t i = 0; i < event_count; ++i) {
                auto& rsynch_context = synch_contexts[i % context_count];
                rsynch_context.push(make_event(GenericEvents::Raise, i));
            }
        }
        {
            auto                          end  = std::chrono::steady_clock::now();
            std::chrono::duration<double> diff = end - start;
            solid_log(logger, Verbose, "Duration after push " << diff.count());
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
