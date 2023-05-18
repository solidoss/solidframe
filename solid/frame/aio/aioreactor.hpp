// solid/frame/aio/reactor.hpp
//
// Copyright (c) 2014, 2023 Valentin Palade (vipalade @ gmail . com)
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
struct EventHandler;
class SocketBase;
class Listener;
template <class Socket>
class Stream;
template <class Socket>
class Datagram;
using ActorPointerT = std::shared_ptr<Actor>;

extern const LoggerT logger;

namespace impl {
class Reactor : public frame::ReactorBase {
    friend struct solid::frame::aio::EventHandler;
    friend class solid::frame::aio::CompletionHandler;
    friend class solid::frame::aio::SocketBase;
    friend class solid::frame::aio::Listener;
    template <class Socket>
    friend class solid::frame::aio::Stream;
    template <class Socket>
    friend class solid::frame::aio::Datagram;
    friend struct ChangeTimerIndexCallback;
    friend struct TimerCallback;
    friend struct solid::frame::aio::ReactorContext;
    friend class solid::frame::aio::Actor;

    struct Data;
    PimplT<Data> impl_;

protected:
#ifdef SOLID_FRAME_AIO_REACTOR_USE_SPINLOCK
    using MutexT = solid::SpinLock;
#else
    using MutexT = mutex;
#endif
    uint32_t           current_wake_index_ = 0;
    uint32_t           current_push_index_ = 0;
    size_t             actor_count_        = 0;
    size_t             current_exec_size_  = 0;
    std::atomic_size_t current_push_size_  = {0};
    std::atomic_size_t current_wake_size_  = {0};

public:
    using EventFunctionT = solid_function_t(void(ReactorContext&, EventBase&&));

    bool start();
    void stop() override;
    void run();

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
                    EventFunctionT eventfnc(std::move(*this));
                    _rctx.reactor().doPost(_rctx, std::move(eventfnc), std::move(_revent));
                } else {
                    function(_rctx, std::move(_revent));
                    _rctx.reactor().doStopActor(_rctx);
                }
            }
        };

        StopActorF     stopfnc{std::move(_f)};
        EventFunctionT eventfnc(std::move(stopfnc));
        doPost(_rctx, std::move(eventfnc), std::move(_uev));
    }

    bool addDevice(ReactorContext& _rctx, Device const& _rsd, const ReactorWaitRequestE _req);

    bool modDevice(ReactorContext& _rctx, Device const& _rsd, const ReactorWaitRequestE _req);

    bool remDevice(CompletionHandler const& _rch, Device const& _rsd);

    bool addTimer(CompletionHandler const& _rch, NanoTime const& _rt, size_t& _rstoreidx);
    bool remTimer(CompletionHandler const& _rch, size_t const& _rstoreidx);

    void addConnect(ReactorContext& _rctx);
    void remConnect(ReactorContext& _rctx);

    void registerCompletionHandler(CompletionHandler& _rch, Actor const& _ract);
    void unregisterCompletionHandler(CompletionHandler& _rch);

    Service& service(ReactorContext const& _rctx) const;

    Actor& actor(ReactorContext const& _rctx) const;

    CompletionHandler* completionHandler(ReactorContext const& _rctx) const;

    void     pushFreeUids();
    UniqueId popUid(Actor&);
    void     notifyOne();
    MutexT&  mutex();
    UniqueId actorUid(ReactorContext const& _rctx) const;

    void addActor(UniqueId const& _uid, Service& _rservice, ActorPointerT&& _actor_ptr);
    bool isValid(UniqueId const& _actor_uid, UniqueId const& _completion_handler_uid) const;

    ReactorContext context(NanoTime const& _rcrttime)
    {
        return ReactorContext(*this, _rcrttime);
    }

    void update(ReactorContext& _rctx, const size_t _completion_handler_index, const size_t _actor_index) const
    {
        _rctx.completion_heandler_index_ = _completion_handler_index;
        _rctx.actor_index_               = _actor_index;
    }

    static void call_actor_on_event(ReactorContext& _rctx, EventBase&& _uevent);
    static void increase_event_vector_size(ReactorContext& _rctx, EventBase&& _uevent);
    static void stop_actor(ReactorContext& _rctx, EventBase&& _uevent);
    static void stop_actor_repost(ReactorContext& _rctx, EventBase&& _uevent);

private:
    static Reactor* safeSpecific();
    static Reactor& specific();

    void doCompleteIo(NanoTime const& _rcrttime, const size_t _sz);
    void doCompleteTimer(NanoTime const& _rcrttime);
    void doCompleteEvents(ReactorContext const& _rctx);
    void doCompleteEvents(NanoTime const& _rcrttime);
    void doStoreSpecific();
    void doClearSpecific();
    void doUpdateTimerIndex(const size_t _chidx, const size_t _newidx, const size_t _oldidx);

    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, EventBase&& _uevent);
    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, EventBase&& _uevent, CompletionHandler const& _rch);

    virtual void   doPostActorStop(ReactorContext& _rctx, const UniqueId& _completion_handler_uid)                                   = 0;
    virtual void   doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, EventBase&& _uev, const UniqueId& _completion_handler_uid) = 0;
    virtual void   doStopActorRepost(ReactorContext& _rctx, const UniqueId& _completion_handler_uid)                                 = 0;
    virtual size_t doCompleteExec(NanoTime const& _rcrttime)                                                                         = 0;
    virtual void   doCompleteEvents(NanoTime const& _rcrttime, const UniqueId& _completion_handler_uid)                              = 0;

    void doStopActor(ReactorContext& _rctx);

    void onTimer(ReactorContext& _rctx, const size_t _chidx);
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
        UniqueId const& _ruid, EventBase&& _revent)
        : actor_uid_(_ruid)
        , event_(std::move(_revent))
    {
    }

    ExecStub(
        UniqueId const& _ruid, EventBase const& _revent = Event<>())
        : actor_uid_(_ruid)
        , event_(_revent)
    {
    }

    ExecStub(
        ExecStub&& _other) noexcept
        : actor_uid_(_other.actor_uid_)
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
        : uid_(_ruid)
        , event_(std::move(_uevent))
    {
    }

    WakeStub(const WakeStub&) = delete;

    WakeStub(
        WakeStub&& _other) noexcept
        : uid_(_other.uid_)
        , event_(std::move(_other.event_))
    {
    }
};

template <class Evnt>
struct PushStub {
    UniqueId      uid_;
    ActorPointerT actor_ptr_;
    Service&      rservice_;
    Evnt          event_;

    PushStub(
        UniqueId const& _ruid, ActorPointerT&& _ractptr, Service& _rsvc, EventBase const& _revent)
        : uid_(_ruid)
        , actor_ptr_(std::move(_ractptr))
        , rservice_(_rsvc)
        , event_(_revent)
    {
    }
    PushStub(
        UniqueId const& _ruid, ActorPointerT&& _ractptr, Service& _rsvc, EventBase&& _revent)
        : uid_(_ruid)
        , actor_ptr_(std::move(_ractptr))
        , rservice_(_rsvc)
        , event_(std::move(_revent))
    {
    }
};
} // namespace impl

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
    using ActorT = Actor;
    using EventT = Evnt;

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
            std::lock_guard<MutexT> lock(mutex());
            const UniqueId          uid     = this->popUid(*_ract);
            auto&                   rpush_q = push_q_[current_push_index_];
            notify                          = rpush_q.empty();
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
        solid_log(logger, Verbose, (void*)this << " uid = " << _ractuid.index << ',' << _ractuid.unique << " event = " << _revent);
        bool notify = false;
        {
            std::lock_guard<MutexT> lock(mutex());
            auto&                   rwake_q = wake_q_[current_wake_index_];
            notify                          = rwake_q.empty();
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
        solid_log(logger, Verbose, (void*)this << " uid = " << _ractuid.index << ',' << _ractuid.unique << " event = " << _revent);
        bool notify = false;
        {
            std::lock_guard<MutexT> lock(mutex());
            auto&                   rwake_q = wake_q_[current_wake_index_];
            notify                          = rwake_q.empty();
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
        exec_q_.push(ExecStubT(actorUid(_rctx), std::move(_uev)));
        exec_q_.back().exec_fnc_               = std::move(_revent_fnc);
        exec_q_.back().completion_handler_uid_ = _completion_handler_uid;
        current_exec_size_                     = exec_q_.size();
    }

    void doStopActorRepost(ReactorContext& _rctx, const UniqueId& _completion_handler_uid)
    {
        exec_q_.push(ExecStubT(actorUid(_rctx)));
        exec_q_.back().exec_fnc_               = &stop_actor;
        exec_q_.back().completion_handler_uid_ = _completion_handler_uid;
        current_exec_size_                     = exec_q_.size();
    }
    /*NOTE:
        We do not stop the actor rightaway - we make sure that any
        pending Events are delivered to the actor before we stop
    */
    void doPostActorStop(ReactorContext& _rctx, const UniqueId& _completion_handler_uid) override
    {
        exec_q_.push(ExecStubT(actorUid(_rctx)));
        exec_q_.back().exec_fnc_               = &stop_actor_repost;
        exec_q_.back().completion_handler_uid_ = _completion_handler_uid;
        current_exec_size_                     = exec_q_.size();
    }

    void doCompleteEvents(NanoTime const& _rcrttime, const UniqueId& _completion_handler_uid)
    {
        solid_log(logger, Verbose, "");

        if (current_push_size_.load() || current_wake_size_.load()) {
            std::lock_guard<MutexT> lock(mutex());
            current_wake_index_ = ((current_wake_index_ + 1) & 1);
            current_push_index_ = ((current_push_index_ + 1) & 1);
            current_push_size_.store(0);
            current_wake_size_.store(0);

            pushFreeUids();
        }

        PushQueueT&    push_q = push_q_[(current_push_index_ + 1) & 1];
        WakeQueueT&    wake_q = wake_q_[(current_wake_index_ + 1) & 1];
        ReactorContext ctx(context(_rcrttime));

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

    size_t doCompleteExec(NanoTime const& _rcrttime) override
    {
        ReactorContext ctx(context(_rcrttime));
        size_t         sz = exec_q_.size();
        const size_t   rv = sz;

        while ((sz--) != 0) {
            auto& rexec(exec_q_.front());
            solid_log(logger, Verbose, sz << " qsz = " << exec_q_.size());
            if (isValid(rexec.actor_uid_, rexec.completion_handler_uid_)) {
                ctx.clearError();
                update(ctx, static_cast<size_t>(rexec.completion_handler_uid_.index), static_cast<size_t>(rexec.actor_uid_.index));
                rexec.exec_fnc_(ctx, std::move(rexec.event_));
            }
            exec_q_.pop();
            current_exec_size_ = exec_q_.size();
        }
        return rv;
    }
};

constexpr size_t reactor_default_event_small_size = std::max(sizeof(Function<void()>), sizeof(std::function<void()>));
using ReactorEventT                               = Event<reactor_default_event_small_size>;
using ReactorT                                    = Reactor<ReactorEventT>;

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
