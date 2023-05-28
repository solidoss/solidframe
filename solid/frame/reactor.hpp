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
#include "solid/system/log.hpp"
#include "solid/system/pimpl.hpp"
#include "solid/utility/event.hpp"
#include "solid/utility/queue.hpp"
#include <atomic>
#include <mutex>
#if !defined(__cpp_lib_atomic_wait)
#include "solid/utility/atomic_wait"
#endif

namespace solid {

struct NanoTime;

namespace frame {

class Actor;
class Service;
class ReactorContext;
struct ChangeTimerIndexCallback;
struct TimerCallback;

extern const LoggerT frame_logger;

using ActorPointerT = std::shared_ptr<Actor>;

namespace impl {

class Reactor : public frame::ReactorBase {
    friend class solid::frame::CompletionHandler;
    friend class solid::frame::ReactorContext;
    friend class solid::frame::Actor;

    struct Data;
    PimplT<Data> impl_;

protected:
    const size_t       wake_capacity_;
    std::atomic_size_t push_wake_index_    = {0};
    std::atomic_size_t pop_wake_index_     = {0};
    std::atomic_size_t pending_wake_count_ = {0};
    uint32_t           current_push_index_ = 0;
    size_t             actor_count_        = 0;
    size_t             current_push_size_  = 0;
    size_t             current_exec_size_  = 0;

public:
    using EventFunctionT = solid_function_t(void(ReactorContext&, EventBase&&));

    bool start();
    void stop() override;
    void run();

protected:
    Reactor(SchedulerBase& _rsched, const size_t _schedidx, const size_t _wake_capacity);
    ~Reactor();

    size_t pushWakeIndex() noexcept
    {
        return push_wake_index_.fetch_add(1) % wake_capacity_;
    }

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

        StopActorF     stopfnc{std::move(_f)};
        EventFunctionT eventfnc(stopfnc);
        doPost(_rctx, std::move(eventfnc), std::move(_uev));
    }

    bool addTimer(CompletionHandler const& _rch, NanoTime const& _rt, size_t& _rstoreidx);
    bool remTimer(CompletionHandler const& _rch, size_t const& _rstoreidx);

    void registerCompletionHandler(CompletionHandler& _rch, Actor const& _ract);
    void unregisterCompletionHandler(CompletionHandler& _rch);

    Service& service(ReactorContext const& _rctx) const;

    Actor&   actor(ReactorContext const& _rctx) const;
    UniqueId actorUid(ReactorContext const& _rctx) const;

    CompletionHandler* completionHandler(ReactorContext const& _rctx) const;

    std::mutex& mutex();
    void        notifyOne();

    void addActor(UniqueId const& _uid, Service& _rservice, ActorPointerT&& _actor_ptr);
    bool isValid(UniqueId const& _actor_uid, UniqueId const& _completion_handler_uid) const;

    UniqueId       popUid(Actor&);
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
    static void stop_actor_repost(ReactorContext& _rctx, EventBase&& _uevent);
    static void stop_actor(ReactorContext& _rctx, EventBase&& _uevent);

private:
    static Reactor* safeSpecific();
    static Reactor& specific();

    bool doWaitEvent(NanoTime const& _rcrttime, const bool _exec_q_empty);

    void doCompleteTimer(NanoTime const& _rcrttime);
    void doStoreSpecific();
    void doClearSpecific();

    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, EventBase&& _uev);
    void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, EventBase&& _uev, CompletionHandler const& _rch);

    virtual void doPostActorStop(ReactorContext& _rctx, const UniqueId& _completion_handler_uid)                                   = 0;
    virtual void doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, EventBase&& _uev, const UniqueId& _completion_handler_uid) = 0;
    virtual void doStopActorRepost(ReactorContext& _rctx, const UniqueId& _completion_handler_uid)                                 = 0;
    virtual void doCompleteExec(NanoTime const& _rcrttime)                                                                         = 0;
    virtual void doCompleteEvents(NanoTime const& _rcrttime, const UniqueId& _completion_handler_uid)                              = 0;

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
enum struct LockE : uint8_t {
    Empty = 0,
    Filled,
    Stop,
    Wake,
};

struct WakeStubBase {
#if defined(__cpp_lib_atomic_wait)
    std::atomic_flag pushing_ = ATOMIC_FLAG_INIT;
    std::atomic_flag popping_ = ATOMIC_FLAG_INIT;
#else
    std::atomic_bool pushing_ = {false};
    std::atomic_bool popping_ = {false};
#endif
    std::atomic_uint8_t lock_ = {to_underlying(LockE::Empty)};

    void waitWhilePush() noexcept
    {
        while (true) {
#if defined(__cpp_lib_atomic_wait)
            const bool already_pushing = pushing_.test_and_set(std::memory_order_acquire);
#else
            bool       expected        = false;
            const bool already_pushing = !pushing_.compare_exchange_strong(expected, true, std::memory_order_acquire);
#endif
            if (!already_pushing) {
                //  wait for lock to be 0.
                auto value = lock_.load();
                if (value != to_underlying(LockE::Empty)) {

                    do {
                        std::atomic_wait(&lock_, value);
                        value = lock_.load();
                    } while (value != to_underlying(LockE::Empty));
                }
                return;
            } else {
#if defined(__cpp_lib_atomic_wait)
                pushing_.wait(true);
#else
                std::atomic_wait(&pushing_, true);
#endif
            }
        }
    }

    void notifyWhilePush() noexcept
    {
        lock_.store(to_underlying(LockE::Filled));
#if defined(__cpp_lib_atomic_wait)
        pushing_.clear(std::memory_order_release);
        pushing_.notify_one();
#else
        pushing_.store(false, std::memory_order_release);
        std::atomic_notify_one(&pushing_);
#endif
    }

    void notifyWhilePop() noexcept
    {
        lock_.store(to_underlying(LockE::Empty));
        std::atomic_notify_one(&lock_);
    }

    bool isFilled() const noexcept
    {
        return lock_.load() == to_underlying(LockE::Filled);
    }
};

template <class Evnt>
struct WakeStub : WakeStubBase {
    UniqueId uid_;
    Evnt     event_;

    void clear() noexcept
    {
        uid_.clear();
        event_.reset();
    }

    void reset(UniqueId const& _ruid, EventBase const& _revent)
    {
        uid_   = _ruid;
        event_ = _revent;
    }

    void reset(UniqueId const& _ruid, EventBase&& _uevent)
    {
        uid_   = _ruid;
        event_ = std::move(_uevent);
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
    using PushStubT  = impl::PushStub<Evnt>;
    using WakeStubT  = impl::WakeStub<Evnt>;
    using ExecQueueT = Queue<ExecStubT>;
    using PushQueueT = Queue<PushStubT>;

    ExecQueueT                   exec_q_;
    PushQueueT                   push_q_[2];
    std::unique_ptr<WakeStubT[]> wake_arr_;

public:
    using ActorT = Actor;
    using EventT = Evnt;

    Reactor(SchedulerBase& _rsched, const size_t _sched_idx, const size_t _wake_capacity)
        : impl::Reactor(_rsched, _sched_idx, _wake_capacity)
        , wake_arr_(new WakeStubT[_wake_capacity])
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
            const auto index = pushWakeIndex();
            auto&      rstub = wake_arr_[index];

            rstub.waitWhilePush();

            rstub.reset(_ractuid, _revent);

            notify = pending_wake_count_.fetch_add(1) == 0;

            rstub.notifyWhilePush();
        }
        if (notify) {
            std::lock_guard<std::mutex> lock(mutex());
            notifyOne();
        }
        return true;
    }
    bool wake(UniqueId const& _ractuid, EventBase&& _revent) override
    {
        bool notify = false;
        {
            const auto index = pushWakeIndex();
            auto&      rstub = wake_arr_[index];

            rstub.waitWhilePush();

            rstub.reset(_ractuid, std::move(_revent));

            notify = pending_wake_count_.fetch_add(1) == 0;

            rstub.notifyWhilePush();
        }
        if (notify) {
            std::lock_guard<std::mutex> lock(mutex());
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

    void doStopActorRepost(ReactorContext& _rctx, const UniqueId& _completion_handler_uid) override
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

    void doCompleteEvents(NanoTime const& _rcrttime, const UniqueId& _completion_handler_uid) override
    {
        solid_log(frame_logger, Verbose, "");

        PushQueueT&    push_q = push_q_[(current_push_index_ + 1) & 1];
        ReactorContext ctx(context(_rcrttime));

        actor_count_ += push_q.size();

        while (!push_q.empty()) {
            auto& rstub = push_q.front();

            addActor(rstub.uid_, rstub.rservice_, std::move(rstub.actor_ptr_));

            exec_q_.push(ExecStubT(rstub.uid_, &call_actor_on_event, _completion_handler_uid, std::move(rstub.event_)));
            push_q.pop();
            current_exec_size_ = exec_q_.size();
        }

        while (true) {
            const size_t index = pop_wake_index_.load() % wake_capacity_;
            auto&        rstub = wake_arr_[index];
            if (rstub.isFilled()) {
                exec_q_.push(ExecStubT(rstub.uid_, &call_actor_on_event, _completion_handler_uid, std::move(rstub.event_)));
                --pending_wake_count_;
                ++pop_wake_index_;
                rstub.clear();
                rstub.notifyWhilePop();
            } else {
                break;
            }
        }

#if false
        while (!wake_q.empty()) {
            auto& rstub = wake_q.front();

            exec_q_.push(ExecStubT(rstub.uid_, &call_actor_on_event, _completion_handler_uid, std::move(rstub.event_)));
            wake_q.pop();
            current_exec_size_ = exec_q_.size();
        }
#endif
    }

    void doCompleteExec(NanoTime const& _rcrttime) override
    {
        ReactorContext ctx(context(_rcrttime));
        size_t         sz = exec_q_.size();

        while ((sz--) != 0) {
            auto& rexec(exec_q_.front());

            if (isValid(rexec.actor_uid_, rexec.completion_handler_uid_)) {
                ctx.clearError();
                update(ctx, static_cast<size_t>(rexec.completion_handler_uid_.index), static_cast<size_t>(rexec.actor_uid_.index));
                rexec.exec_fnc_(ctx, std::move(rexec.event_));
            }
            exec_q_.pop();
            current_exec_size_ = exec_q_.size();
        }
    }
};

constexpr size_t reactor_default_event_small_size = std::max(sizeof(Function<void()>), sizeof(std::function<void()>));
using ReactorEventT                               = Event<reactor_default_event_small_size>;
using ReactorT                                    = Reactor<ReactorEventT>;

} // namespace frame
} // namespace solid
