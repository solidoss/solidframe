#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

#include "solid/frame/actor.hpp"
#include "solid/frame/reactor.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"
#include "solid/utility/event.hpp"

using namespace solid;
using namespace std;
namespace {
const LoggerT logger("test");

using SchedulerT = frame::Scheduler<frame::Reactor<Event<128>>>;
atomic<size_t> received_events{0};
atomic<size_t> accumulate_value{0};

class Actor final : public frame::Actor {
    promise<void>& rprom_;

    void onEvent(frame::ReactorContext& _rctx, EventBase&& _revent) override
    {
        if (generic_event<GenericEventE::Start> == _revent) {
        } else if (generic_event<GenericEventE::Wake> == _revent) {
            accumulate_value += *_revent.cast<size_t>();
            if (received_events.fetch_sub(1) == 1) {
                rprom_.set_value();
            }
        } else if (generic_event<GenericEventE::Kill> == _revent) {
            postStop(_rctx);
        }
    }

public:
    Actor(promise<void>& _rprom)
        : rprom_(_rprom)
    {
    }
};

} // namespace

int test_perf_actor_frame(int argc, char* argv[])
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

    received_events = event_count;

    auto lambda = [&]() {
        SchedulerT              scheduler;
        frame::Manager          manager;
        frame::ServiceT         service{manager};
        promise<void>           prom;
        vector<frame::UniqueId> actors;

        scheduler.start(thread_count);

        for (size_t i = 0; i < context_count; ++i) {
            ErrorConditionT err;
            actors.emplace_back(scheduler.startActor(make_shared<Actor>(prom), service, make_event(GenericEventE::Start), err));
            solid_check(!err);
        }

        for (size_t i = 0; i < event_count; ++i) {
            manager.notify(actors[i % actors.size()], make_event(GenericEventE::Wake, i));
        }

        auto fut = prom.get_future();
        solid_check(fut.wait_for(chrono::seconds(wait_seconds)) == future_status::ready);
        fut.get();
    };

    auto fut = async(launch::async, lambda);
    if (fut.wait_for(chrono::seconds(wait_seconds)) != future_status::ready) {

        solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
    }
    fut.get();

    solid_log(logger, Verbose, "after async wait " << received_events << " " << accumulate_value);

    return 0;
}
