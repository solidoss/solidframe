#include "solid/frame/actor.hpp"
#include "solid/frame/manager.hpp"
#include "solid/frame/reactor.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"
#include "solid/frame/timer.hpp"

#include "solid/system/cassert.hpp"
#include "solid/system/log.hpp"
#include <condition_variable>
#include <mutex>

#include "solid/utility/event.hpp"

#include <iostream>

using namespace solid;
using namespace std;

namespace {
condition_variable cnd;
bool               running = true;
mutex              mtx;
} // namespace

// enum Events{
//  EventStartE = 0,
//  EventRunE,
//  EventStopE,
//  EventSendE,
// };

typedef frame::Scheduler<frame::Reactor> SchedulerT;

class BasicActor : public Dynamic<BasicActor, frame::Actor> {
public:
    BasicActor(size_t _repeat = 10)
        : repeat(_repeat)
        , t1(proxy())
        , t2(proxy())
    {
    }

private:
    void onEvent(frame::ReactorContext& _rctx, Event&& _revent) override;
    void onTimer(frame::ReactorContext& _rctx, size_t _idx);

private:
    size_t             repeat;
    frame::SteadyTimer t1;
    frame::SteadyTimer t2;
};

int main(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:VIEW"});

    {
        SchedulerT s;

        frame::Manager  m;
        frame::ServiceT svc(m);

        s.start(1);

        {
            const size_t cnt = argc == 2 ? atoi(argv[1]) : 1;

            for (size_t i = 0; i < cnt; ++i) {
                solid::ErrorConditionT err;
                solid::frame::ActorIdT actuid;

                actuid = s.startActor(make_dynamic<BasicActor>(10), svc, make_event(GenericEvents::Start), err);
                solid_log(generic_logger, Info, "Started BasicActor: " << actuid.index << ',' << actuid.unique);
            }

            {
                unique_lock<mutex> lock(mtx);
                while (running) {
                    cnd.wait(lock);
                }
            }
        }
        m.stop();
    }
    return 0;
}

/*virtual*/ void BasicActor::onEvent(frame::ReactorContext& _rctx, Event&& _uevent)
{
    solid_log(generic_logger, Info, "event = " << _uevent);
    if (_uevent == generic_event_start) {
        t1.waitUntil(_rctx, _rctx.steadyTime() + std::chrono::seconds(5), [this](frame::ReactorContext& _rctx) { return onTimer(_rctx, 0); });
        t2.waitUntil(_rctx, _rctx.steadyTime() + std::chrono::seconds(10), [this](frame::ReactorContext& _rctx) { return onTimer(_rctx, 1); });
    } else if (_uevent == generic_event_kill) {
        postStop(_rctx);
    }
}

void BasicActor::onTimer(frame::ReactorContext& _rctx, size_t _idx)
{
    solid_log(generic_logger, Info, "timer = " << _idx);
    if (_idx == 0) {
        if (repeat--) {
            t2.cancel(_rctx);
            t1.waitUntil(_rctx, _rctx.steadyTime() + std::chrono::seconds(5), [this](frame::ReactorContext& _rctx) { return onTimer(_rctx, 0); });
            solid_assert(!_rctx.error());
            t2.waitUntil(_rctx, _rctx.steadyTime() + std::chrono::seconds(10), [this](frame::ReactorContext& _rctx) { return onTimer(_rctx, 1); });
            solid_assert(!_rctx.error());
        } else {
            t2.cancel(_rctx);
            lock_guard<mutex> lock(mtx);
            running = false;
            cnd.notify_one();
            postStop(_rctx);
        }
    } else if (_idx == 1) {
        cout << "ERROR: second timer should never fire" << endl;
        solid_assert(false);
    } else {
        cout << "ERROR: unknown timer index: " << _idx << endl;
        solid_assert(false);
    }
}

/*virtual*/ /*void BasicActor::execute(frame::ExecuteContext &_rexectx){
    switch(_rexectx.event().id){
        case frame::EventInit:
            cout<<"EventInit("<<_rexectx.event().index<<") at "<<_rexectx.time()<<endl;
            //t1 should fire first
            t1.waitUntil(_rexectx, _rexectx.time() + 1000 * 5, frame::EventTimer, 1);
            solid_assert(!_rexectx.error());
            t2.waitUntil(_rexectx, _rexectx.time() + 1000 * 10, frame::EventTimer, 2);
            solid_assert(!_rexectx.error());
            break;
        case frame::EventTimer:
            cout<<"EventTimer("<<_rexectx.event().index<<") at "<<_rexectx.time()<<endl;
            if(_rexectx.event().index == 1){
                if(repeat--){
                    t2.cancel(_rexectx);
                    t1.waitUntil(_rexectx, _rexectx.time() + 1000 * 5, frame::EventTimer, 1);
                    solid_assert(!_rexectx.error());
                    t2.waitUntil(_rexectx, _rexectx.time() + 1000 * 10, frame::EventTimer, 2);
                    solid_assert(!_rexectx.error());
                }else{
                    t2.cancel(_rexectx);
                    Locker<Mutex>   lock(mtx);
                    running = false;
                    cnd.signal();
                }
            }else if(_rexectx.event().index == 2){
                cout<<"ERROR: second timer should never fire"<<endl;
                solid_assert(false);
            }else{
                cout<<"ERROR: unknown timer index"<<endl;
                solid_assert(false);
            }
        case frame::EventDie:
            _rexectx.die();
            break;
        default:
            break;
    }
    if(_rexectx.error()){
        _rexectx.die();
    }
}*/
