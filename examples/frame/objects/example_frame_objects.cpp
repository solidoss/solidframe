#include "solid/frame/manager.hpp"
#include "solid/frame/object.hpp"
#include "solid/frame/reactor.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"
#include "solid/frame/timer.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include "solid/system/cassert.hpp"
#include "solid/system/debug.hpp"

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

class BasicObject : public Dynamic<BasicObject, frame::Object> {
    enum Status : uint8_t {
        StatusInMemory,
        StatusCompressed,
        StatusOnDisk
    };

public:
    BasicObject()
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

using ObjectDequeT = std::deque<BasicObject>;

namespace {
ObjectDequeT       objdq;
condition_variable cnd;
mutex              mtx;
size_t             running_objcnt = 0;
size_t             started_objcnt = 0;
size_t             ondisk_objcnt  = 0;
} // namespace

int main(int argc, char* argv[])
{
#ifdef SOLID_HAS_DEBUG
    {
        string dbgout;
        Debug::the().levelMask("view");
        Debug::the().moduleMask("");

        Debug::the().initStdErr(
            false,
            &dbgout);

        cout << "Debug output: " << dbgout << endl;
        dbgout.clear();
        Debug::the().moduleNames(dbgout);
        cout << "Debug modules: " << dbgout << endl;
    }
#endif

    {
        SchedulerT s;

        frame::Manager         m;
        frame::ServiceT        svc(m);
        solid::ErrorConditionT err;

        err = s.start(1);

        if (!err) {
            const size_t cnt = argc == 2 ? atoi(argv[1]) : 1000;

            cout << "Creating " << cnt << " objects:" << endl;

            for (size_t i = 0; i < cnt; ++i) {

                objdq.emplace_back();

                DynamicPointer<frame::Object> objptr(&objdq.back());
                solid::frame::ObjectIdT       objuid;
                {
                    unique_lock<mutex> lock(mtx);

                    objuid = s.startObject(objptr, svc, make_event(GenericEvents::Start), err);

                    idbg("Started BasicObject: " << objuid.index << ',' << objuid.unique);

                    if (err) {
                        cout << "Error starting object " << i << ": " << err.message() << endl;
                        break;
                    }
                    ++running_objcnt;
                }
            }
        } else {
            cout << "Error starting scheduler: " << err.message() << endl;
        }

        if (!err) {
            {
                unique_lock<mutex> lock(mtx);
                while (started_objcnt != running_objcnt) {
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
                while (ondisk_objcnt != running_objcnt) {
                    cnd.wait(lock);
                }
            }
            cout << "All objects on disk" << endl;
        }
    }
    cout << "DONE!" << endl;
    return 0;
}

/*virtual*/ void BasicObject::onEvent(frame::ReactorContext& _rctx, Event&& _uevent)
{
    idbg("event = " << _uevent);
    if (_uevent == generic_event_start) {
        {
            unique_lock<mutex> lock(mtx);
            ++started_objcnt;
            if (started_objcnt == running_objcnt) {
                cnd.notify_one();
            }
        }
    } else if (_uevent == generic_event_kill) {
        postStop(_rctx);
    } else if (_uevent == generic_event_raise) {
        if (status_ != StatusOnDisk) {
            status_ = StatusOnDisk;
            timer_.cancel(_rctx);
            unique_lock<mutex> lock(mtx);
            ++ondisk_objcnt;
            if (ondisk_objcnt == running_objcnt) {
                cnd.notify_one();
            }
        }
    } else if (_uevent == generic_event_update) {
        active_ = true;
        timer_.waitFor(_rctx, std::chrono::seconds(10) + std::chrono::milliseconds(this->id() % 1000), [this](frame::ReactorContext& _rctx) { return onTimer(_rctx); });
    }
}

void BasicObject::onTimer(frame::ReactorContext& _rctx)
{
    idbg("");

    if (status_ == StatusInMemory) {
        status_ = StatusCompressed;
        timer_.waitFor(_rctx, std::chrono::seconds(10) + std::chrono::milliseconds(this->id() % 1000), [this](frame::ReactorContext& _rctx) { return onTimer(_rctx); });
    } else if (status_ == StatusCompressed) {
        status_ = StatusOnDisk;
        unique_lock<mutex> lock(mtx);
        ++ondisk_objcnt;
        if (ondisk_objcnt == running_objcnt) {
            cnd.notify_one();
        }
    }
}
