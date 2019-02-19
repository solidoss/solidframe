#include "solid/frame/actor.hpp"
#include "solid/frame/manager.hpp"
#include "solid/frame/reactor.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"
#include "solid/frame/timer.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include "solid/system/cassert.hpp"
#include "solid/system/log.hpp"

#include "solid/utility/event.hpp"

#include <iostream>

using namespace solid;
using namespace std;

using SchedulerT = frame::Scheduler<frame::Reactor>;

struct GlobalId {
    GlobalId()
        : index(-1)
        , unique(-1)
        , group_index(-1)
        , group_unique(-1)
    {
    }

    uint64_t index;
    uint32_t unique;
    uint16_t group_index;
    uint16_t group_unique;
};

class BasicActor : public Dynamic<BasicActor, frame::Actor> {
    enum Status : uint8_t {
        StatusInMemory,
        StatusCompressed,
        StatusOnDisk
    };

public:
    BasicActor()
        : timer_(proxy())
        , status_(StatusInMemory)
        , active_(false)
    {
        DynamicBase::use();
    }

private:
    void onEvent(frame::ReactorContext& _rctx, Event&& _revent) override;
    void onTimer(frame::ReactorContext& _rctx);

private:
    frame::SteadyTimer timer_;
    Status             status_;
    bool               active_;
    GlobalId           id_;
};

using ActorDequeT = std::deque<BasicActor>;

namespace {
ActorDequeT        actdq;
condition_variable cnd;
mutex              mtx;
size_t             running_actcnt = 0;
size_t             started_actcnt = 0;
size_t             ondisk_actcnt  = 0;
} // namespace

int main(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:VIEW"});

    {
        SchedulerT s;

        frame::Manager         m;
        frame::ServiceT        svc(m);
        solid::ErrorConditionT err;

        s.start(1);

        {
            const size_t cnt = argc == 2 ? atoi(argv[1]) : 1000;

            cout << "Creating " << cnt << " actors:" << endl;

            for (size_t i = 0; i < cnt; ++i) {

                actdq.emplace_back();

                DynamicPointer<frame::Actor> actptr(&actdq.back());
                solid::frame::ActorIdT       actuid;
                {
                    lock_guard<mutex> lock(mtx);

                    actuid = s.startActor(actptr, svc, make_event(GenericEvents::Start), err);

                    solid_log(generic_logger, Info, "Started BasicActor: " << actuid.index << ',' << actuid.unique);

                    if (err) {
                        cout << "Error starting actor " << i << ": " << err.message() << endl;
                        break;
                    }
                    ++running_actcnt;
                }
            }
        }

        if (!err) {
            {
                unique_lock<mutex> lock(mtx);
                while (started_actcnt != running_actcnt) {
                    cnd.wait(lock);
                }
            }

            cout << "Notify all update: START" << endl;
            auto start_time = std::chrono::steady_clock::now();

            svc.notifyAll(generic_event_update);

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);

            cout << "Notify all update: DONE. " << duration.count() << "ms" << endl;

            this_thread::sleep_for(chrono::seconds(20));

            cout << "Notify all raise: START" << endl;
            start_time = std::chrono::steady_clock::now();
            svc.notifyAll(generic_event_raise);
            duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
            cout << "Notify all raise: DONE. " << duration.count() << "ms" << endl;
            {
                unique_lock<mutex> lock(mtx);
                while (ondisk_actcnt != running_actcnt) {
                    cnd.wait(lock);
                }
            }
            cout << "All actors on disk" << endl;
        }
    }
    cout << "DONE!" << endl;
    return 0;
}

/*virtual*/ void BasicActor::onEvent(frame::ReactorContext& _rctx, Event&& _uevent)
{
    solid_log(generic_logger, Info, "event = " << _uevent);
    if (_uevent == generic_event_start) {
        {
            lock_guard<mutex> lock(mtx);
            ++started_actcnt;
            if (started_actcnt == running_actcnt) {
                cnd.notify_one();
            }
        }
    } else if (_uevent == generic_event_kill) {
        postStop(_rctx);
    } else if (_uevent == generic_event_raise) {
        if (status_ != StatusOnDisk) {
            status_ = StatusOnDisk;
            timer_.cancel(_rctx);
            lock_guard<mutex> lock(mtx);
            ++ondisk_actcnt;
            if (ondisk_actcnt == running_actcnt) {
                cnd.notify_one();
            }
        }
    } else if (_uevent == generic_event_update) {
        active_ = true;
        timer_.waitFor(_rctx, std::chrono::seconds(10) + std::chrono::milliseconds(this->id() % 1000), [this](frame::ReactorContext& _rctx) { return onTimer(_rctx); });
    }
}

void BasicActor::onTimer(frame::ReactorContext& _rctx)
{
    solid_log(generic_logger, Info, "");

    if (status_ == StatusInMemory) {
        status_ = StatusCompressed;
        timer_.waitFor(_rctx, std::chrono::seconds(10) + std::chrono::milliseconds(this->id() % 1000), [this](frame::ReactorContext& _rctx) { return onTimer(_rctx); });
    } else if (status_ == StatusCompressed) {
        status_ = StatusOnDisk;
        lock_guard<mutex> lock(mtx);
        ++ondisk_actcnt;
        if (ondisk_actcnt == running_actcnt) {
            cnd.notify_one();
        }
    }
}
