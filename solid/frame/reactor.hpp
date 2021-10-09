// solid/frame/reactor.hpp
//
// Copyright (c) 2007, 2008, 2013,2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/common.hpp"
#include "solid/frame/reactorbase.hpp"
#include "solid/frame/reactorcontext.hpp"
#include "solid/system/pimpl.hpp"

namespace solid {
struct NanoTime;
namespace frame {

class Service;
class Actor;
struct ReactorContext;
class CompletionHandler;
struct ChangeTimerIndexCallback;
struct TimerCallback;

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
            : repost(true)
            , function{std::forward<Function>(_function)}
        {
        }

        void operator()(ReactorContext& _rctx, Event&& _revent)
        {
            if (repost) { //skip one round - to guarantee that all remaining posts were delivered
                repost = false;
                EventFunctionT eventfnc(*this);
                _rctx.reactor().doPost(_rctx, std::move(eventfnc), std::move(_revent));
            } else {
                function(_rctx, _revent);
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
    void post(ReactorContext& _rctx, Function _fnc, Event&& _uev)
    {
        EventFunctionT eventfnc(std::move(_fnc));
        doPost(_rctx, std::move(eventfnc), std::move(_uev));
    }

    template <typename Function>
    void post(ReactorContext& _rctx, Function&& _f, Event&& _uev, CompletionHandler const& _rch)
    {
        EventFunctionT eventfnc{std::forward<Function>(_f)};
        doPost(_rctx, std::move(eventfnc), std::move(_uev), _rch);
    }

    void postActorStop(ReactorContext& _rctx);

    template <typename Function>
    void postActorStop(ReactorContext& _rctx, Function&& _f, Event&& _uev)
    {
        using RealF = typename std::decay<Function>::type;
        StopActorF<RealF> stopfnc{std::forward<RealF>(_f)};
        EventFunctionT    eventfnc(stopfnc);
        doPost(_rctx, std::move(eventfnc), std::move(_uev));
    }

    bool addTimer(CompletionHandler const& _rch, NanoTime const& _rt, size_t& _rstoreidx);
    bool remTimer(CompletionHandler const& _rch, size_t const& _rstoreidx);

    bool start();

    bool raise(UniqueId const& _ractuid, Event const& _revt) override;
    bool raise(UniqueId const& _ractuid, Event&& _uevt) override;
    void stop() override;

    void registerCompletionHandler(CompletionHandler& _rch, Actor const& _ract);
    void unregisterCompletionHandler(CompletionHandler& _rch);

    void run();
    bool push(TaskT&& _ract, Service& _rsvc, Event const& _revt);

    Service& service(ReactorContext const& _rctx) const;

    Actor&   actor(ReactorContext const& _rctx) const;
    UniqueId actorUid(ReactorContext const& _rctx) const;

    CompletionHandler* completionHandler(ReactorContext const& _rctx) const;

private:
    friend class CompletionHandler;
    friend struct ChangeTimerIndexCallback;
    friend struct TimerCallback;
    friend struct ExecStub;

    static Reactor* safeSpecific();
    static Reactor& specific();

    bool doWaitEvent(NanoTime const& _rcrttime);

    void doCompleteTimer(NanoTime const& _rcrttime);
    void doCompleteExec(NanoTime const& _rcrttime);
    void doCompleteEvents(NanoTime const& _rcrttime);
    void doStoreSpecific();
    void doClearSpecific();
    void doUpdateTimerIndex(const size_t _chidx, const size_t _newidx, const size_t _oldidx);

    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, Event&& _uev);
    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, Event&& _uev, CompletionHandler const& _rch);

    void doStopActor(ReactorContext& _rctx);

    void        onTimer(ReactorContext& _rctx, const size_t _tidx, const size_t _chidx);
    static void call_actor_on_event(ReactorContext& _rctx, Event&& _uevent);
    static void stop_actor_repost(ReactorContext& _rctx, Event&& _uevent);
    static void stop_actor(ReactorContext& _rctx, Event&& _uevent);

private: //data
    struct Data;
    PimplT<Data> impl_;
};

} //namespace frame
} //namespace solid
