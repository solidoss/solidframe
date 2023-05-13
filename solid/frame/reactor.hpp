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
#include <mutex>
#include "solid/frame/common.hpp"
#include "solid/frame/reactorbase.hpp"
#include "solid/frame/reactorcontext.hpp"
#include "solid/system/pimpl.hpp"

namespace solid {
struct NanoTime;
namespace frame {

class Service;
class Actor;
class ReactorContext;
class CompletionHandler;
struct ChangeTimerIndexCallback;
struct TimerCallback;

using ActorPointerT = std::shared_ptr<Actor>;

namespace impl{

class Reactor : public frame::ReactorBase{
    friend class CompletionHandler;
    friend class ReactorContext;

    using EventFunctionT = solid_function_t(void(ReactorContext&, EventBase&&)) ;
    struct Data;
    PimplT<Data> impl_;
protected:
    size_t  actor_count_ = 0;
protected:
    Reactor(SchedulerBase& _rsched, const size_t _schedidx);
    ~Reactor();

    template <typename Function>
    void post(ReactorContext& _rctx, Function _fnc, EventBase&& _uev)
    {
        EventFunctionT eventfnc(std::move(_fnc));
        doPost(_rctx, std::move(eventfnc), std::move(_uev));
    }

    template <typename Function>
    void post(ReactorContext& _rctx, Function&& _f, EventBase&& _uev, CompletionHandler const& _rch)
    {
        EventFunctionT eventfnc{std::forward<Function>(_f)};
        doPost(_rctx, std::move(eventfnc), std::move(_uev), _rch);
    }

    void postActorStop(ReactorContext& _rctx);

    template <typename Function>
    void postActorStop(ReactorContext& _rctx, Function&& _f, EventBase&& _uev)
    {
        using RealF = typename std::decay<Function>::type;
        StopActorF<RealF> stopfnc{std::forward<RealF>(_f)};
        EventFunctionT    eventfnc(stopfnc);
        doPost(_rctx, std::move(eventfnc), std::move(_uev));
    }

    bool addTimer(CompletionHandler const& _rch, NanoTime const& _rt, size_t& _rstoreidx);
    bool remTimer(CompletionHandler const& _rch, size_t const& _rstoreidx);

    bool start();

    void stop() override;

    void registerCompletionHandler(CompletionHandler& _rch, Actor const& _ract);
    void unregisterCompletionHandler(CompletionHandler& _rch);

    void run();

    Service& service(ReactorContext const& _rctx) const;

    Actor&   actor(ReactorContext const& _rctx) const;
    UniqueId actorUid(ReactorContext const& _rctx) const;

    CompletionHandler* completionHandler(ReactorContext const& _rctx) const;

    std::mutex& mutex();
    void notifyOne();

    void addActor(UniqueId const&_uid, Service &_rservice, ActorPointerT &&_actor_ptr);
    bool isValid(UniqueId const&_actor_uid, UniqueId const&_completion_handler_uid)const;
private:

    static Reactor* safeSpecific();
    static Reactor& specific();

    bool doWaitEvent(NanoTime const& _rcrttime);

    void doCompleteTimer(NanoTime const& _rcrttime);
    void doCompleteExec(NanoTime const& _rcrttime);
    void doCompleteEvents(NanoTime const& _rcrttime);
    void doStoreSpecific();
    void doClearSpecific();

    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, EventBase&& _uev);
    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, EventBase&& _uev, CompletionHandler const& _rch);
    
    virtual void doPostActorStop(ReactorContext& _rctx, const UniqueId &_completion_handler_uid) = 0;
    virtual void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, EventBase&& _uev, const UniqueId &_completion_handler_uid) = 0;
    virtual void doStopActorRepost(ReactorContext& _rctx, const UniqueId &_completion_handler_uid) = 0;

    void doStopActor(ReactorContext& _rctx);

    void        onTimer(ReactorContext& _rctx, const size_t _chidx);
    static void call_actor_on_event(ReactorContext& _rctx, EventBase&& _uevent);
    static void stop_actor_repost(ReactorContext& _rctx, EventBase&& _uevent);
    static void stop_actor(ReactorContext& _rctx, EventBase&& _uevent);
};

template <class Evnt>
struct ExecStub {
    UniqueId                actor_uid_;
    UniqueId                completion_handler_uid_;
    Reactor::EventFunctionT exec_fnc_;
    Evnt                    event_;

    template <class F>
    ExecStub(
        UniqueId const& _ruid, F _f, EventBase&& _uevent = Event<>())
        : actor_uid_(_ruid)
        , exec_fnc_(_f)
        , event_(std::move(_uevent))
    {
    }

    template <class F>
    ExecStub(
        UniqueId const& _ruid, F _f, UniqueId const& _rchnuid, EventBase&& _revent = Event<>())
        : actor_uid_(_ruid)
        , completion_handler_uid_(_rchnuid)
        , exec_fnc_(std::move(_f))
        , event_(std::move(_revent))
    {
    }

    ExecStub(
        UniqueId const& _ruid, Event&& _revent)
        : actor_uid_(_ruid)
        , event_(std::move(_revent))
    {
    }

    ExecStub(
        UniqueId const& _ruid, Event const& _revent = Event())
        : actor_uid_(_ruid)
        , event_(_revent)
    {
    }

    ExecStub(
        ExecStub&& _other) noexcept
        : actor_uid_(_other.actuid)
        , completion_handler_uid_(_other.completion_handler_uid_)
        , event_(std::move(_other.event_))
    {
        std::swap(exec_fnc_, _other.exec_fnc_);
    }
};

template <class Evnt>
struct WakeStub {
    UniqueId uid_;
    Evnt     event_;
    
    WakeStub(
        UniqueId const& _ruid, EventBase const& _revent)
        : uid_(_ruid)
        , event_(_revent)
    {
    }

    WakeStub(
        UniqueId const& _ruid, EventBase&& _uevent)
        : uid(_ruid)
        , event_(std::move(_uevent))
    {
    }

    WakeStub(const WakeStub&) = delete;

    WakeStub(
        WakeStub&& _other) noexcept
        : uid_(_other.uid)
        , event_(std::move(_other.event))
    {
    }
};

template <class Evnt>
struct PushStub {
    UniqueId uid_;
    ActorPointerT    actor_ptr_;
    Service& rservice_;
    Evnt    event_;

    PushStub(
        UniqueId const& _ruid, ActorPointerT&& _ractptr, Service& _rsvc, EventBase const& _revent)
        : uid_(_ruid)
        , actor_ptr_(std::move(_ractptr))
        , rservice_(_rsvc)
        , event_(_revent)
    {
    }
    PushStub(
        UniqueId const& _ruid, ActorPointerT&& _ractptr, Service& _rsvc, EventBase && _revent)
        : uid_(_ruid)
        , actor_ptr_(std::move(_ractptr))
        , rservice_(_rsvc)
        , event_(std::move(_revent))
    {
    }
};

}//namespace impl

template <class Evnt>
class Reactor: public impl::Reactor{
    using ExecStubT = impl::ExecStub<Evnt>;
    using WakeStubT = impl::WakeStub<Evnt>;
    using PushStubT = impl::PushStub<Evnt>;
    using ExecQueueT = Queue<ExecStubT>;
    using WakeQueueT = Queue<WakeStubT>;
    using PushQueueT = Queue<PushStubT>;
    
    ExecQueueT exec_q_;
    WakeQueueT wake_q_[2];
    PushQueueT push_q_[2];
    uint32_t   current_wake_index_ = 0;
    uint32_t   current_push_index_ = 0;
public:
    Reactor(SchedulerBase& _rsched, const size_t _sched_idx): impl::Reactor(_rsched, _sched_idx){}

    bool push(ActorPointerT&& _ract, Service& _rsvc, EventBase const& _revt)
    {
        bool notify = false;
        {
            std::lock_guard<std::mutex> lock(mutex());
            const UniqueId    uid = this->popUid(*_ract);
            auto & rpush_q = push_q_[current_push_index_];
            notify = rpush_q.empty();
            rpush_q.(PushStub(uid, std::move(_ract), _rsvc, _revent));
        }
        if(notify){
            notifyOne();
        }
        return true;
    }

    bool push(ActorPointerT&& _ract, Service& _rsvc, EventBase&& _revt)
    {
        bool notify = false;
        {
            std::lock_guard<std::mutex> lock(mutex());
            const UniqueId    uid = this->popUid(*_ract);
            auto & rpush_q = push_q_[current_push_index_];
            notify = rpush_q.empty();
            rpush_q.(PushStub(uid, std::move(_ract), _rsvc, std::move(_revent)));
        }
        if(notify){
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
            auto & rwake_q = wake_q_[current_wake_index_];
            notify = rwake_q.empty();
            rwake_q.push(WakeStub(_ractuid, _revent));
        }
        if(notify){
            notifyOne();
        }
        return true;
    }
    bool wake(UniqueId const& _ractuid, EventBase&& _revent) override
    {
        bool notify = false;
        {
            std::lock_guard<std::mutex> lock(mutex());
            auto & rwake_q = wake_q_[current_wake_index_];
            notify = rwake_q.empty();
            rwake_q.push(WakeStub(_ractuid, std::move(_revent)));
        }
        if(notify){
            notifyOne();
        }
        return true;
    }

    void doPost(ReactorContext& _rctx, EventFunctionT&& _revent_fnc, EventBase&& _uev, const UniqueId &_completion_handler_uid) override{
        exec_q_.push(ExecStubT(_rctx.actorUid(), std::move(_uev)));
        exec_q_.back().exec_fnc_ = std::move(_revent_fnc);
        exec_q_.back().completion_handler_uid_ = _completion_handler_uid;
    }

    void Reactor::doStopActorRepost(ReactorContext& _rctx, const UniqueId &_completion_handler_uid)
    {
        exec_q_.push(ExecStubT(_rctx.actorUid()));
        exec_q_.back().exefnc = &stop_actor;
        exec_q_.back().completion_handler_uid_ = _completion_handler_uid;
    }
    /*NOTE:
        We do not stop the actor rightaway - we make sure that any
        pending Events are delivered to the actor before we stop
    */
    void doPostActorStop(ReactorContext& _rctx, const UniqueId &_completion_handler_uid) override
    {
        exec_q_.push(ExecStubT(_rctx.actorUid()));
        exec_q_.back().exec_fnc_ = &stop_actor_repost;
        exec_q_.back().completion_handler_uid_ = _completion_handler_uid;
    }

    void doCompleteEvents(NanoTime const& _rcrttime, const UniqueId &_completion_handler_uid)
    {
        solid_log(frame_logger, Verbose, "");

        PushQueueT&    push_q = push_q_[(current_push_index_ + 1) & 1];
        WakeQueueT&    wake_q = wake_q_[(current_wale_index_ + 1) & 1];
        ReactorContext     ctx(*this, _rcrttime);
        
        incrementActorCount(push_q.size());

        actor_count_ += push_q.size();

        while(!push_q.empty()) {
            auto &rstub = push_q.front();
            
            addActor(rstub.uid_, rstub.rservice_, std::move(rstub.actor_ptr_));

            exec_q_.push(ExecStubT(rstub.uid_, &call_actor_on_event, _completion_handler_uid, std::move(rstub.event_)));
            push_q.pop();
        }

        while(!wake_q.empty())
        {
            auto &rstub = wake_q.front();

            exec_q_.push(ExecStubT(rstub.uid_, &call_actor_on_event, _completion_handler_uid, std::move(rstub.event_)));

            wake_q.pop();
        }
    }

    void doCompleteExec(NanoTime const& _rcrttime)
    {
        ReactorContext ctx(*this, _rcrttime);
        size_t         sz = exec_q_.size();

        while ((sz--) != 0) {
            auto& rexec(exec_q_.front());

            if(isValid(rexec.actor_uid_, rexec.completion_handler_uid_)){
                ctx.clearError();
                ctx.completion_heandler_idx_ = static_cast<size_t>(rexec.completion_handler_uid_.index);
                ctx.actor_idx_ = static_cast<size_t>(rexec.actor_uid_.index);
                rexec.exec_fnc_(ctx, std::move(rexec.event_));
            }
            exec_q_.pop();
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
            : repost(true)
            , function{std::forward<Function>(_function)}
        {
        }

        void operator()(ReactorContext& _rctx, Event&& _revent)
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

    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, Event&& _uev);
    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, Event&& _uev, CompletionHandler const& _rch);

    void doStopActor(ReactorContext& _rctx);

    void        onTimer(ReactorContext& _rctx, const size_t _chidx);
    static void call_actor_on_event(ReactorContext& _rctx, Event&& _uevent);
    static void stop_actor_repost(ReactorContext& _rctx, Event&& _uevent);
    static void stop_actor(ReactorContext& _rctx, Event&& _uevent);

private: // data
    struct Data;
    PimplT<Data> impl_;
};
#endif

} // namespace frame
} // namespace solid
