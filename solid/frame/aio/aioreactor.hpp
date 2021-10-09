// solid/frame/aio/reactor.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/aio/aiocommon.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/frame/common.hpp"
#include "solid/frame/reactorbase.hpp"
#include "solid/system/nanotime.hpp"
#include "solid/system/pimpl.hpp"

namespace solid {

struct NanoTime;
class Device;
struct Event;

namespace frame {

class Service;

namespace aio {

class Actor;
class CompletionHandler;
struct ChangeTimerIndexCallback;
struct TimerCallback;
struct EventHandler;
struct ExecStub;

using ActorPointerT = std::shared_ptr<Actor>;

//!
/*!

*/
class Reactor : public frame::ReactorBase {
    typedef solid_function_t(void(ReactorContext&, Event&&)) EventFunctionT;

    template <class Function>
    struct StopActorF {
        Function function;
        bool     repost;

        explicit StopActorF(Function&& _function)
            : function{std::forward<Function>(_function)}
            , repost(true)
        {
        }

        void operator()(ReactorContext& _rctx, Event&& _uevent)
        {
            if (repost) { //skip one round - to guarantee that all remaining posts were delivered
                repost = false;
                EventFunctionT eventfnc{std::move(*this)};
                _rctx.reactor().doPost(_rctx, std::move(eventfnc), std::move(_uevent));
            } else {
                function(_rctx, std::move(_uevent));
                _rctx.reactor().doStopActor(_rctx);
            }
        }
    };

public:
    typedef ActorPointerT TaskT;
    typedef Actor         ActorT;

    Reactor(SchedulerBase& _rsched, const size_t _schedidx);
    ~Reactor();

    template <typename Function>
    void post(ReactorContext& _rctx, Function&& _fnc, Event&& _uev)
    {
        EventFunctionT eventfnc{std::forward<Function>(_fnc)};
        doPost(_rctx, std::move(eventfnc), std::move(_uev));
    }

    template <typename Function>
    void post(ReactorContext& _rctx, Function&& _fnc, Event&& _uev, CompletionHandler const& _rch)
    {
        EventFunctionT eventfnc{std::forward<Function>(_fnc)};
        doPost(_rctx, std::move(eventfnc), std::move(_uev), _rch);
    }

    void postActorStop(ReactorContext& _rctx);

    template <typename Function>
    void postActorStop(ReactorContext& _rctx, Function&& _f, Event&& _uev)
    {
        using RealF = typename std::decay<Function>::type;
        StopActorF<RealF> stopfnc{std::forward<RealF>(_f)};
        EventFunctionT    eventfnc(std::move(stopfnc));
        doPost(_rctx, std::move(eventfnc), std::move(_uev));
    }

    bool addDevice(ReactorContext& _rctx, Device const& _rsd, const ReactorWaitRequestsE _req);

    bool modDevice(ReactorContext& _rctx, Device const& _rsd, const ReactorWaitRequestsE _req);

    bool remDevice(CompletionHandler const& _rch, Device const& _rsd);

    bool addTimer(CompletionHandler const& _rch, NanoTime const& _rt, size_t& _rstoreidx);
    bool remTimer(CompletionHandler const& _rch, size_t const& _rstoreidx);

    void addConnect(ReactorContext& _rctx);
    void remConnect(ReactorContext& _rctx);

    bool start();

    bool raise(UniqueId const& _ractuid, Event const& _revt) override;
    bool raise(UniqueId const& _ractuid, Event&& _uevt) override;
    void stop() override;

    void registerCompletionHandler(CompletionHandler& _rch, Actor const& _ract);
    void unregisterCompletionHandler(CompletionHandler& _rch);

    void run();
    bool push(TaskT&& _ract, Service& _rsvc, Event&& _revt);

    Service& service(ReactorContext const& _rctx) const;

    Actor& actor(ReactorContext const& _rctx) const;

    CompletionHandler* completionHandler(ReactorContext const& _rctx) const;

private:
    friend struct EventHandler;
    friend class CompletionHandler;
    friend struct ChangeTimerIndexCallback;
    friend struct TimerCallback;
    friend struct ExecStub;
    friend struct ReactorContext;

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

    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, Event&& _uev);
    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, Event&& _uev, CompletionHandler const& _rch);

    void doStopActor(ReactorContext& _rctx);

    void        onTimer(ReactorContext& _rctx, const size_t _tidx, const size_t _chidx);
    static void call_actor_on_event(ReactorContext& _rctx, Event&& _uev);
    static void increase_event_vector_size(ReactorContext& _rctx, Event&& _uev);
    static void stop_actor(ReactorContext& _rctx, Event&& _uevent);
    static void stop_actor_repost(ReactorContext& _rctx, Event&& _uevent);

    UniqueId actorUid(ReactorContext const& _rctx) const;

private: //data
    struct Data;
    PimplT<Data> impl_;
};

//-----------------------------------------------------------------------------

} //namespace aio
} //namespace frame
} //namespace solid
