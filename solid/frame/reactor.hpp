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
#include <mutex>

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

struct ReactorStatistic : ReactorStatisticBase {
    std::atomic_uint64_t push_notify_count_;
    std::atomic_uint64_t push_count_;
    std::atomic_uint64_t wake_notify_count_;
    std::atomic_uint64_t wake_count_;
    std::atomic_uint64_t post_count_;
    std::atomic_uint64_t post_stop_count_;
    std::atomic_size_t   max_exec_size_;
    std::atomic_size_t   actor_count_;
    std::atomic_size_t   max_actor_count_;

    void actorCount(const size_t _count)
    {
        actor_count_ = _count;
        solid_statistic_max(max_actor_count_, _count);
    }
    void pushNotify()
    {
        ++push_notify_count_;
    }

    void push()
    {
        ++push_count_;
    }

    void wakeNotify()
    {
        ++wake_notify_count_;
    }

    void wake()
    {
        ++wake_count_;
    }

    void post()
    {
        ++post_count_;
    }

    void postStop()
    {
        ++post_stop_count_;
    }
    void execSize(const size_t _sz)
    {
        solid_statistic_max(max_exec_size_, _sz);
    }

    std::ostream& print(std::ostream& _ros) const override;

    void clear();
};

namespace impl {

class Reactor : public frame::ReactorBase {
    friend class solid::frame::CompletionHandler;
    friend class solid::frame::ReactorContext;
    friend class solid::frame::Actor;

    struct Data;
    Pimpl<Data, 656> impl_;

protected:
    const size_t       wake_capacity_;
    ReactorStatistic&  rstatistic_;
    size_t             actor_count_        = 0;
    size_t             current_exec_size_  = 0;
    size_t             pop_wake_index_     = {0};
    std::atomic_size_t push_wake_index_    = {0};
    std::atomic_size_t pending_wake_count_ = {0};

public:
    using StatisticT     = ReactorStatistic;
    using EventFunctionT = solid_function_t(void(ReactorContext&, EventBase&&));

    bool start();
    void stop() override;
    void run();

protected:
    Reactor(SchedulerBase& _rsched, StatisticT& _rstatistic, const size_t _schedidx, const size_t _wake_capacity);
    ~Reactor();

    std::tuple<AtomicIndexValueT, AtomicCounterValueT> pushWakeIndex() noexcept
    {
        const auto index = push_wake_index_.fetch_add(1);
        return {index % wake_capacity_, computeCounter(index, wake_capacity_)};
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

template <class Evnt>
struct WakeStub : WakeStubBase {
    UniqueId      uid_;
    Evnt          event_;
    ActorPointerT actor_ptr_;
    Service*      pservice_ = nullptr;

    void clear() noexcept
    {
        uid_.clear();
        event_.reset();
        actor_ptr_.reset();
        pservice_ = nullptr;
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

    void reset(UniqueId const& _ruid, EventBase const& _revent, ActorPointerT&& _actor_ptr, Service* _pservice)
    {
        uid_       = _ruid;
        event_     = _revent;
        actor_ptr_ = std::move(_actor_ptr);
        pservice_  = _pservice;
    }

    void reset(UniqueId const& _ruid, EventBase&& _uevent, ActorPointerT&& _actor_ptr, Service* _pservice)
    {
        uid_       = _ruid;
        event_     = std::move(_uevent);
        actor_ptr_ = std::move(_actor_ptr);
        pservice_  = _pservice;
    }
};

} // namespace impl

template <class Evnt>
class Reactor : public impl::Reactor {
    using ExecStubT  = impl::ExecStub<Evnt>;
    using WakeStubT  = impl::WakeStub<Evnt>;
    using ExecQueueT = Queue<ExecStubT>;

    ExecQueueT                   exec_q_;
    std::unique_ptr<WakeStubT[]> wake_arr_;

public:
    using ActorT = Actor;
    using EventT = Evnt;

    Reactor(SchedulerBase& _rsched, StatisticT& _rstatistic, const size_t _sched_idx, const size_t _wake_capacity)
        : impl::Reactor(_rsched, _rstatistic, _sched_idx, _wake_capacity)
        , wake_arr_(new WakeStubT[wake_capacity_])
    {
    }

    bool push(ActorPointerT&& _ract, Service& _rsvc, EventBase const& _revent)
    {
        bool notify = false;
        {
            mutex().lock();
            const UniqueId uid = this->popUid(*_ract);
            mutex().unlock();
            const auto index = pushWakeIndex();
            auto&      rstub = wake_arr_[index];

            rstub.waitWhilePush();

            rstub.reset(uid, _revent, std::move(_ract), &_rsvc);

            notify = pending_wake_count_.fetch_add(1) == 0;

            rstub.notifyWhilePush();
        }
        if (notify) {
            {
                std::lock_guard<std::mutex> lock(mutex());
                notifyOne();
            }
            rstatistic_.pushNotify();
        }
        rstatistic_.push();
        return true;
    }

    bool push(ActorPointerT&& _ract, Service& _rsvc, EventBase&& _revent)
    {
        bool notify = false;
        {
            mutex().lock();
            const UniqueId uid = this->popUid(*_ract);
            mutex().unlock();
            const auto [index, count] = pushWakeIndex();
            auto& rstub               = wake_arr_[index];

            rstub.waitWhilePush(rstatistic_, count);

            rstub.reset(uid, std::move(_revent), std::move(_ract), &_rsvc);

            notify = pending_wake_count_.fetch_add(1) == 0;

            rstub.notifyWhilePush();
        }

        if (notify) {
            {
                std::lock_guard<std::mutex> lock(mutex());
                notifyOne();
            }
            rstatistic_.pushNotify();
        }
        rstatistic_.push();
        return true;
    }

private:
    bool wake(UniqueId const& _ractuid, EventBase const& _revent) override
    {
        bool notify = false;
        {
            const auto [index, count] = pushWakeIndex();
            auto& rstub               = wake_arr_[index];

            rstub.waitWhilePush(rstatistic_, count);

            rstub.reset(_ractuid, _revent);

            notify = pending_wake_count_.fetch_add(1) == 0;

            rstub.notifyWhilePush();
        }
        if (notify) {
            {
                std::lock_guard<std::mutex> lock(mutex());
                notifyOne();
            }
            rstatistic_.wakeNotify();
        }
        rstatistic_.wake();
        return true;
    }
    bool wake(UniqueId const& _ractuid, EventBase&& _revent) override
    {
        bool notify = false;
        {
            const auto [index, count] = pushWakeIndex();
            auto& rstub               = wake_arr_[index];

            rstub.waitWhilePush(rstatistic_, count);

            rstub.reset(_ractuid, std::move(_revent));

            notify = pending_wake_count_.fetch_add(1) == 0;

            rstub.notifyWhilePush();
        }
        if (notify) {
            {
                std::lock_guard<std::mutex> lock(mutex());
                notifyOne();
                rstatistic_.wakeNotify();
            }
        }
        rstatistic_.wake();
        return true;
    }

    void doPost(ReactorContext& _rctx, EventFunctionT&& _revent_fnc, EventBase&& _uev, const UniqueId& _completion_handler_uid) override
    {
        exec_q_.push(ExecStubT(actorUid(_rctx), std::move(_uev)));
        exec_q_.back().exec_fnc_               = std::move(_revent_fnc);
        exec_q_.back().completion_handler_uid_ = _completion_handler_uid;
        current_exec_size_                     = exec_q_.size();
        rstatistic_.post();
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
        rstatistic_.postStop();
    }

    void doCompleteEvents(NanoTime const& _rcrttime, const UniqueId& _completion_handler_uid) override
    {
        solid_log(frame_logger, Verbose, "");

        ReactorContext ctx(context(_rcrttime));

        while (true) {
            const size_t index = pop_wake_index_ % wake_capacity_;
            auto&        rstub = wake_arr_[index];
            if (rstub.isFilled(pop_wake_index_, wake_capacity_)) {
                if (rstub.actor_ptr_) [[unlikely]] {
                    ++actor_count_;
                    rstatistic_.actorCount(actor_count_);
                    addActor(rstub.uid_, *rstub.pservice_, std::move(rstub.actor_ptr_));
                }
                exec_q_.push(ExecStubT(rstub.uid_, &call_actor_on_event, _completion_handler_uid, std::move(rstub.event_)));
                --pending_wake_count_;
                ++pop_wake_index_;
                rstub.clear();
                rstub.notifyWhilePop();
            } else {
                break;
            }
        }
    }

    void doCompleteExec(NanoTime const& _rcrttime) override
    {
        ReactorContext ctx(context(_rcrttime));
        size_t         sz = exec_q_.size();

        rstatistic_.execSize(sz);

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
