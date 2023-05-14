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
#include "solid/utility/event.hpp"
#include "solid/utility/queue.hpp"

namespace solid {

struct NanoTime;
class Device;

namespace frame {

class Service;

namespace aio {

using ActorPointerT = std::shared_ptr<Actor>;

namespace impl{
class Reactor : public frame::ReactorBase {
    friend struct EventHandler;
    friend class CompletionHandler;
    friend struct ChangeTimerIndexCallback;
    friend struct TimerCallback;
    friend struct ReactorContext;

    using EventFunctionT = solid_function_t(void(ReactorContext&, EventBase&&));
    struct Data;
    PimplT<Data> impl_;

protected:
    uint32_t current_wake_index_ = 0;
    uint32_t current_push_index_ = 0;
    size_t   actor_count_        = 0;
    size_t   current_push_size_  = 0;
    size_t   current_wake_size_  = 0;
    size_t   current_exec_size_  = 0;

protected:
    Reactor(SchedulerBase& _rsched, const size_t _schedidx);
    ~Reactor();

    template <typename Function>
    void post(ReactorContext& _rctx, Function&& _fnc, EventBase&& _uev)
    {
        EventFunctionT eventfnc{std::forward<Function>(_fnc)};
        doPost(_rctx, std::move(eventfnc), std::move(_uev));
    }

    template <typename Function>
    void post(ReactorContext& _rctx, Function&& _fnc, EventBase&& _uev, CompletionHandler const& _rch)
    {
        EventFunctionT eventfnc{std::forward<Function>(_fnc)};
        doPost(_rctx, std::move(eventfnc), std::move(_uev), _rch);
    }

    void postActorStop(ReactorContext& _rctx);

    template <typename Function>
    void postActorStop(ReactorContext& _rctx, Function&& _f, EventBase&& _uev)
    {
        using RealF = typename std::decay<Function>::type;
        
        struct StopActorF {
            Function function;
            bool     repost = true;

            explicit StopActorF(Function&& _function)
                : function{std::move(_function)}
            {
            }

            void operator()(ReactorContext& _rctx, EventBase&& _revent)
            {
                if (repost) { // skip one round - to guarantee that all remaining posts were delivered
                    repost = false;
                    EventFunctionT eventfnc(*this);
                    _rctx.reactor().doPost(_rctx, std::move(eventfnc), std::move(_revent));
                } else {
                    function(_rctx, _revent);
                    _rctx.reactor().doStopActor(_rctx);
                }
            }
        };

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

    void stop() override;

    void registerCompletionHandler(CompletionHandler& _rch, Actor const& _ract);
    void unregisterCompletionHandler(CompletionHandler& _rch);

    void run();

    Service& service(ReactorContext const& _rctx) const;

    Actor& actor(ReactorContext const& _rctx) const;

    CompletionHandler* completionHandler(ReactorContext const& _rctx) const;

private:

    static Reactor* safeSpecific();
    static Reactor& specific();

    void   doCompleteIo(NanoTime const& _rcrttime, const size_t _sz);
    void   doCompleteTimer(NanoTime const& _rcrttime);
    size_t doCompleteExec(NanoTime const& _rcrttime);
    void   doCompleteEvents(ReactorContext const& _rctx);
    void   doCompleteEvents(NanoTime const& _rcrttime);
    void   doStoreSpecific();
    void   doClearSpecific();
    void   doUpdateTimerIndex(const size_t _chidx, const size_t _newidx, const size_t _oldidx);

    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, EventBase&& _uevent);
    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, EventBase&& _uevent, CompletionHandler const& _rch);

    void doStopActor(ReactorContext& _rctx);

    void        onTimer(ReactorContext& _rctx, const size_t _chidx);
    static void call_actor_on_event(ReactorContext& _rctx, EventBase&& _uevent);
    static void increase_event_vector_size(ReactorContext& _rctx, EventBase&& _uevent);
    static void stop_actor(ReactorContext& _rctx, EventBase&& _uevent);
    static void stop_actor_repost(ReactorContext& _rctx, EventBase&& _uevent);

    UniqueId actorUid(ReactorContext const& _rctx) const;
};
}//namespace impl


template <class Evnt>
class Reactor : public impl::Reactor {
    using ExecStubT  = impl::ExecStub<Evnt>;
    using WakeStubT  = impl::WakeStub<Evnt>;
    using PushStubT  = impl::PushStub<Evnt>;
    using ExecQueueT = Queue<ExecStubT>;
    using WakeQueueT = Queue<WakeStubT>;
    using PushQueueT = Queue<PushStubT>;

    ExecQueueT exec_q_;
    WakeQueueT wake_q_[2];
    PushQueueT push_q_[2];

public:
    Reactor(SchedulerBase& _rsched, const size_t _sched_idx)
        : impl::Reactor(_rsched, _sched_idx)
    {
    }

    bool push(ActorPointerT&& _ract, Service& _rsvc, EventBase const& _revent)
    {
        bool notify = false;
        {
            std::lock_guard<std::mutex> lock(mutex());
            const UniqueId              uid     = this->popUid(*_ract);
            auto&                       rpush_q = push_q_[current_push_index_];
            notify                              = rpush_q.empty();
            rpush_q.push(PushStubT(uid, std::move(_ract), _rsvc, _revent));
            current_push_size_ = rpush_q.size();
        }
        if (notify) {
            notifyOne();
        }
        return true;
    }

    bool push(ActorPointerT&& _ract, Service& _rsvc, EventBase&& _revent)
    {
        bool notify = false;
        {
            std::lock_guard<std::mutex> lock(mutex());
            const UniqueId              uid     = this->popUid(*_ract);
            auto&                       rpush_q = push_q_[current_push_index_];
            notify                              = rpush_q.empty();
            rpush_q.push(PushStubT(uid, std::move(_ract), _rsvc, std::move(_revent)));
            current_push_size_ = rpush_q.size();
        }
        if (notify) {
            notifyOne();
        }
        return true;
    }

private:
    bool wake(UniqueId const& _ractuid, EventBase const& _revent) override
    {
        bool notify = false;
        {
            std::lock_guard<std::mutex> lock(mutex());
            auto&                       rwake_q = wake_q_[current_wake_index_];
            notify                              = rwake_q.empty();
            rwake_q.push(WakeStubT(_ractuid, _revent));
            current_wake_size_ = rwake_q.size();
        }
        if (notify) {
            notifyOne();
        }
        return true;
    }
    bool wake(UniqueId const& _ractuid, EventBase&& _revent) override
    {
        bool notify = false;
        {
            std::lock_guard<std::mutex> lock(mutex());
            auto&                       rwake_q = wake_q_[current_wake_index_];
            notify                              = rwake_q.empty();
            rwake_q.push(WakeStubT(_ractuid, std::move(_revent)));
            current_wake_size_ = rwake_q.size();
        }
        if (notify) {
            notifyOne();
        }
        return true;
    }

    void doPost(ReactorContext& _rctx, EventFunctionT&& _revent_fnc, EventBase&& _uev, const UniqueId& _completion_handler_uid) override
    {
        exec_q_.push(ExecStubT(_rctx.actorUid(), std::move(_uev)));
        exec_q_.back().exec_fnc_               = std::move(_revent_fnc);
        exec_q_.back().completion_handler_uid_ = _completion_handler_uid;
        current_exec_size_                     = exec_q_.size();
    }

    void doStopActorRepost(ReactorContext& _rctx, const UniqueId& _completion_handler_uid)
    {
        exec_q_.push(ExecStubT(_rctx.actorUid()));
        exec_q_.back().exefnc                  = &stop_actor;
        exec_q_.back().completion_handler_uid_ = _completion_handler_uid;
        current_exec_size_                     = exec_q_.size();
    }
    /*NOTE:
        We do not stop the actor rightaway - we make sure that any
        pending Events are delivered to the actor before we stop
    */
    void doPostActorStop(ReactorContext& _rctx, const UniqueId& _completion_handler_uid) override
    {
        exec_q_.push(ExecStubT(_rctx.actorUid()));
        exec_q_.back().exec_fnc_               = &stop_actor_repost;
        exec_q_.back().completion_handler_uid_ = _completion_handler_uid;
        current_exec_size_                     = exec_q_.size();
    }

    void doCompleteEvents(NanoTime const& _rcrttime, const UniqueId& _completion_handler_uid)
    {
        solid_log(frame_logger, Verbose, "");

        PushQueueT&    push_q = push_q_[(current_push_index_ + 1) & 1];
        WakeQueueT&    wake_q = wake_q_[(current_wake_index_ + 1) & 1];
        ReactorContext ctx(*this, _rcrttime);

        incrementActorCount(push_q.size());

        actor_count_ += push_q.size();

        while (!push_q.empty()) {
            auto& rstub = push_q.front();

            addActor(rstub.uid_, rstub.rservice_, std::move(rstub.actor_ptr_));

            exec_q_.push(ExecStubT(rstub.uid_, &call_actor_on_event, _completion_handler_uid, std::move(rstub.event_)));
            push_q.pop();
            current_exec_size_ = exec_q_.size();
        }

        while (!wake_q.empty()) {
            auto& rstub = wake_q.front();

            exec_q_.push(ExecStubT(rstub.uid_, &call_actor_on_event, _completion_handler_uid, std::move(rstub.event_)));
            wake_q.pop();
            current_exec_size_ = exec_q_.size();
        }
    }

    void doCompleteExec(NanoTime const& _rcrttime) override
    {
        ReactorContext ctx(*this, _rcrttime);
        size_t         sz = exec_q_.size();

        while ((sz--) != 0) {
            auto& rexec(exec_q_.front());

            if (isValid(rexec.actor_uid_, rexec.completion_handler_uid_)) {
                ctx.clearError();
                ctx.completion_heandler_idx_ = static_cast<size_t>(rexec.completion_handler_uid_.index);
                ctx.actor_idx_               = static_cast<size_t>(rexec.actor_uid_.index);
                rexec.exec_fnc_(ctx, std::move(rexec.event_));
            }
            exec_q_.pop();
            current_exec_size_ = exec_q_.size();
        }
    }
};

#if false
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
            if (repost) { // skip one round - to guarantee that all remaining posts were delivered
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

    void   doCompleteIo(NanoTime const& _rcrttime, const size_t _sz);
    void   doCompleteTimer(NanoTime const& _rcrttime);
    size_t doCompleteExec(NanoTime const& _rcrttime);
    void   doCompleteEvents(ReactorContext const& _rctx);
    void   doCompleteEvents(NanoTime const& _rcrttime);
    void   doStoreSpecific();
    void   doClearSpecific();
    void   doUpdateTimerIndex(const size_t _chidx, const size_t _newidx, const size_t _oldidx);

    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, Event&& _uev);
    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, Event&& _uev, CompletionHandler const& _rch);

    void doStopActor(ReactorContext& _rctx);

    void        onTimer(ReactorContext& _rctx, const size_t _chidx);
    static void call_actor_on_event(ReactorContext& _rctx, Event&& _uev);
    static void increase_event_vector_size(ReactorContext& _rctx, Event&& _uev);
    static void stop_actor(ReactorContext& _rctx, Event&& _uevent);
    static void stop_actor_repost(ReactorContext& _rctx, Event&& _uevent);

    UniqueId actorUid(ReactorContext const& _rctx) const;

private: // data
    struct Data;
    PimplT<Data> impl_;
};
#endif
//-----------------------------------------------------------------------------

} // namespace aio
} // namespace frame
} // namespace solid
