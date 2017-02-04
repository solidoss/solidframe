// solid/frame/aio/reactor.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_REACTOR_HPP
#define SOLID_FRAME_AIO_REACTOR_HPP

#include "solid/frame/aio/aiocommon.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/frame/common.hpp"
#include "solid/frame/reactorbase.hpp"
#include "solid/system/nanotime.hpp"
#include "solid/utility/dynamicpointer.hpp"

namespace solid {

struct NanoTime;
class Device;
struct Event;

namespace frame {

class Service;

namespace aio {

class Object;
class CompletionHandler;
struct ChangeTimerIndexCallback;
struct TimerCallback;

typedef DynamicPointer<Object> ObjectPointerT;

//!
/*!

*/
class Reactor : public frame::ReactorBase {
    typedef FUNCTION<void(ReactorContext&, Event&&)> EventFunctionT;

    template <class Function>
    struct StopObjectF {
        Function function;
        bool     repost;

        explicit StopObjectF(Function& _function)
            : function(std::move(_function))
            , repost(true)
        {
        }

        void operator()(ReactorContext& _rctx, Event&& _uevent)
        {
            if (repost) { //skip one round - to guarantee that all remaining posts were delivered
                repost = false;
                EventFunctionT eventfnc(*this);
                _rctx.reactor().doPost(_rctx, eventfnc, std::move(_uevent));
            } else {
                function(_rctx, std::move(_uevent));
                _rctx.reactor().doStopObject(_rctx);
            }
        }
    };

public:
    typedef ObjectPointerT TaskT;
    typedef Object         ObjectT;

    Reactor(SchedulerBase& _rsched, const size_t _schedidx);
    ~Reactor();

    template <typename Function>
    void post(ReactorContext& _rctx, Function _fnc, Event&& _uev)
    {
        EventFunctionT eventfnc(_fnc);
        doPost(_rctx, eventfnc, std::move(_uev));
    }

    template <typename Function>
    void post(ReactorContext& _rctx, Function _fnc, Event&& _uev, CompletionHandler const& _rch)
    {
        EventFunctionT eventfnc(_fnc);
        doPost(_rctx, eventfnc, std::move(_uev), _rch);
    }

    void postObjectStop(ReactorContext& _rctx);

    template <typename Function>
    void postObjectStop(ReactorContext& _rctx, Function _f, Event&& _uev)
    {
        StopObjectF<Function> stopfnc(_f);
        EventFunctionT        eventfnc(stopfnc);
        doPost(_rctx, eventfnc, std::move(_uev));
    }

    bool waitDevice(ReactorContext& _rctx, CompletionHandler const& _rch, Device const& _rsd, const ReactorWaitRequestsE _req);
    bool addDevice(ReactorContext& _rctx, CompletionHandler const& _rch, Device const& _rsd, const ReactorWaitRequestsE _req);
    bool remDevice(CompletionHandler const& _rch, Device const& _rsd);

    bool addTimer(CompletionHandler const& _rch, NanoTime const& _rt, size_t& _rstoreidx);
    bool remTimer(CompletionHandler const& _rch, size_t const& _rstoreidx);

    bool start();

    bool raise(UniqueId const& _robjuid, Event const& _revt) override;
    bool raise(UniqueId const& _robjuid, Event&& _uevt) override;
    void stop() override;

    void registerCompletionHandler(CompletionHandler& _rch, Object const& _robj);
    void unregisterCompletionHandler(CompletionHandler& _rch);

    void run();
    bool push(TaskT& _robj, Service& _rsvc, Event&& _revt);

    Service& service(ReactorContext const& _rctx) const;

    Object& object(ReactorContext const& _rctx) const;
    UniqueId objectUid(ReactorContext const& _rctx) const;

    CompletionHandler* completionHandler(ReactorContext const& _rctx) const;

private:
    friend struct EventHandler;
    friend class CompletionHandler;
    friend struct ChangeTimerIndexCallback;
    friend struct TimerCallback;
    friend struct ExecStub;

    static Reactor* safeSpecific();
    static Reactor& specific();

    void doCompleteIo(NanoTime const& _rcrttime, const size_t _sz);
    void doCompleteTimer(NanoTime const& _rcrttime);
    void doCompleteExec(NanoTime const& _rcrttime);
    void doCompleteEvents(ReactorContext const& _rctx);
    void doCompleteEvents(NanoTime const& _rcrttime);
    void doStoreSpecific();
    void doClearSpecific();
    void doUpdateTimerIndex(const size_t _chidx, const size_t _newidx, const size_t _oldidx);

    void doPost(ReactorContext& _rctx, EventFunctionT& _revfn, Event&& _uev);
    void doPost(ReactorContext& _rctx, EventFunctionT& _revfn, Event&& _uev, CompletionHandler const& _rch);

    void doStopObject(ReactorContext& _rctx);

    void onTimer(ReactorContext& _rctx, const size_t _tidx, const size_t _chidx);
    static void call_object_on_event(ReactorContext& _rctx, Event&& _uev);
    static void increase_event_vector_size(ReactorContext& _rctx, Event&& _uev);
    static void stop_object(ReactorContext& _rctx, Event&& _uevent);
    static void stop_object_repost(ReactorContext& _rctx, Event&& _uevent);

private: //data
    struct Data;
    Data& d;
};

} //namespace aio
} //namespace frame
} //namespace solid

#endif
