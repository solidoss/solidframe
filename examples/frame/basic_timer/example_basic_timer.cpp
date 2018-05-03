#include "solid/frame/manager.hpp"
#include "solid/frame/object.hpp"
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

class BasicObject : public Dynamic<BasicObject, frame::Object> {
public:
    BasicObject(size_t _repeat = 10)
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

        if (!s.start(1)) {
            const size_t cnt = argc == 2 ? atoi(argv[1]) : 1;

            for (size_t i = 0; i < cnt; ++i) {
                DynamicPointer<frame::Object> objptr(new BasicObject(10));
                solid::ErrorConditionT        err;
                solid::frame::ObjectIdT       objuid;

                objuid = s.startObject(objptr, svc, make_event(GenericEvents::Start), err);
                solid_log(basic_logger, Info, "Started BasicObject: " << objuid.index << ',' << objuid.unique);
            }

            {
                unique_lock<mutex> lock(mtx);
                while (running) {
                    cnd.wait(lock);
                }
            }
        } else {
            cout << "Error starting scheduler" << endl;
        }
        m.stop();
    }
    return 0;
}

/*virtual*/ void BasicObject::onEvent(frame::ReactorContext& _rctx, Event&& _uevent)
{
    solid_log(basic_logger, Info, "event = " << _uevent);
    if (_uevent == generic_event_start) {
        t1.waitUntil(_rctx, _rctx.steadyTime() + std::chrono::seconds(5), [this](frame::ReactorContext& _rctx) { return onTimer(_rctx, 0); });
        t2.waitUntil(_rctx, _rctx.steadyTime() + std::chrono::seconds(10), [this](frame::ReactorContext& _rctx) { return onTimer(_rctx, 1); });
    } else if (_uevent == generic_event_kill) {
        postStop(_rctx);
    }
}

void BasicObject::onTimer(frame::ReactorContext& _rctx, size_t _idx)
{
    solid_log(basic_logger, Info, "timer = " << _idx);
    if (_idx == 0) {
        if (repeat--) {
            t2.cancel(_rctx);
            t1.waitUntil(_rctx, _rctx.steadyTime() + std::chrono::seconds(5), [this](frame::ReactorContext& _rctx) { return onTimer(_rctx, 0); });
            SOLID_ASSERT(!_rctx.error());
            t2.waitUntil(_rctx, _rctx.steadyTime() + std::chrono::seconds(10), [this](frame::ReactorContext& _rctx) { return onTimer(_rctx, 1); });
            SOLID_ASSERT(!_rctx.error());
        } else {
            t2.cancel(_rctx);
            lock_guard<mutex> lock(mtx);
            running = false;
            cnd.notify_one();
            postStop(_rctx);
        }
    } else if (_idx == 1) {
        cout << "ERROR: second timer should never fire" << endl;
        SOLID_ASSERT(false);
    } else {
        cout << "ERROR: unknown timer index: " << _idx << endl;
        SOLID_ASSERT(false);
    }
}

/*virtual*/ /*void BasicObject::execute(frame::ExecuteContext &_rexectx){
    switch(_rexectx.event().id){
        case frame::EventInit:
            cout<<"EventInit("<<_rexectx.event().index<<") at "<<_rexectx.time()<<endl;
            //t1 should fire first
            t1.waitUntil(_rexectx, _rexectx.time() + 1000 * 5, frame::EventTimer, 1);
            SOLID_ASSERT(!_rexectx.error());
            t2.waitUntil(_rexectx, _rexectx.time() + 1000 * 10, frame::EventTimer, 2);
            SOLID_ASSERT(!_rexectx.error());
            break;
        case frame::EventTimer:
            cout<<"EventTimer("<<_rexectx.event().index<<") at "<<_rexectx.time()<<endl;
            if(_rexectx.event().index == 1){
                if(repeat--){
                    t2.cancel(_rexectx);
                    t1.waitUntil(_rexectx, _rexectx.time() + 1000 * 5, frame::EventTimer, 1);
                    SOLID_ASSERT(!_rexectx.error());
                    t2.waitUntil(_rexectx, _rexectx.time() + 1000 * 10, frame::EventTimer, 2);
                    SOLID_ASSERT(!_rexectx.error());
                }else{
                    t2.cancel(_rexectx);
                    Locker<Mutex>   lock(mtx);
                    running = false;
                    cnd.signal();
                }
            }else if(_rexectx.event().index == 2){
                cout<<"ERROR: second timer should never fire"<<endl;
                SOLID_ASSERT(false);
            }else{
                cout<<"ERROR: unknown timer index"<<endl;
                SOLID_ASSERT(false);
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
