// solid/frame/src/reactor.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <queue>
#include <thread>
#include <vector>

#include "solid/system/common.hpp"
#include "solid/system/device.hpp"
#include "solid/system/error.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/nanotime.hpp"

#include "solid/utility/event.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"

#include "solid/frame/actor.hpp"
#include "solid/frame/common.hpp"
#include "solid/frame/completion.hpp"
#include "solid/frame/reactor.hpp"
#include "solid/frame/reactorcontext.hpp"
#include "solid/frame/service.hpp"
#include "solid/frame/timer.hpp"
#include "solid/frame/timestore.hpp"

using namespace std;

namespace solid {
namespace frame {

namespace {

void dummy_completion(CompletionHandler&, ReactorContext&)
{
}

struct EventActor : public Actor {
    CompletionHandler dummy_handler_;

    EventActor()
        : dummy_handler_(proxy(), dummy_completion)
    {
    }

    void stop()
    {
        dummy_handler_.deactivate();
        dummy_handler_.unregister();
    }

    template <class F>
    void post(ReactorContext& _rctx, F _f)
    {
        Actor::post(_rctx, _f);
    }
};

struct CompletionHandlerStub {
    CompletionHandler* pcompletion_handler_;
    size_t             actor_idx_;
    UniqueT            unique_ = 0;

    CompletionHandlerStub(
        CompletionHandler* _pch    = nullptr,
        const size_t       _actidx = InvalidIndex())
        : pcompletion_handler_(_pch)
        , actor_idx_(_actidx)
    {
    }
};

struct ActorStub {
    UniqueT       unique_   = 0;
    Service*      pservice_ = nullptr;
    ActorPointerT actor_ptr_;

    void clear()
    {
        actor_ptr_.reset();
        pservice_ = nullptr;
        ++unique_;
    }
};

constexpr size_t min_event_capacity = 32;
constexpr size_t max_event_capacity = 1024 * 64;

using UniqueIdVectorT         = std::vector<UniqueId>;
using CompletionHandlerDequeT = std::deque<CompletionHandlerStub>;
using ActorDequeT             = std::deque<ActorStub>;
using SizeStackT              = Stack<size_t>;
} // namespace

struct impl::Reactor::Data {
    bool                    running_   = false;
    bool                    must_stop_ = false;
    TimeStore               time_store_{max_event_capacity};
    NanoTime                current_time_;
    std::mutex              mutex_;
    condition_variable      cnd_var_;
    shared_ptr<EventActor>  event_actor_ptr_;
    CompletionHandlerDequeT completion_handler_dq_;
    UniqueIdVectorT         freeuid_vec_;
    ActorDequeT             actor_dq_;
    SizeStackT              completion_handler_index_stk_;

    NanoTime computeWaitDuration(NanoTime const& _rcrt, const bool _exec_q_empty) const
    {
        if (!_exec_q_empty) {
            return NanoTime();
        } else if (!time_store_.empty()) {

            if (_rcrt < time_store_.expiry()) {
                const auto crt_tp  = _rcrt.timePointCast<std::chrono::steady_clock::time_point>();
                const auto next_tp = time_store_.expiry().timePointCast<std::chrono::steady_clock::time_point>();
                const auto delta   = next_tp - crt_tp;

                if (delta <= std::chrono::minutes(10)) {
                    return delta;
                } else {
                    return std::chrono::minutes(10);
                }

            } else {
                return NanoTime();
            }
        } else {
            return NanoTime::max();
        }
    }

    UniqueId dummyCompletionHandlerUid() const
    {
        const size_t idx = event_actor_ptr_->dummy_handler_.idxreactor;
        return UniqueId(idx, completion_handler_dq_[idx].unique_);
    }
};

namespace impl {

Reactor::Reactor(
    SchedulerBase& _rsched,
    const size_t   _idx)
    : ReactorBase(_rsched, _idx)
    , impl_(make_pimpl<Data>())
{
    solid_log(frame_logger, Verbose, "");
}

Reactor::~Reactor()
{
    solid_log(frame_logger, Verbose, "");
}

bool Reactor::start()
{
    doStoreSpecific();
    solid_log(frame_logger, Verbose, "");

    impl_->actor_dq_.push_back(ActorStub());
    impl_->actor_dq_.back().actor_ptr_ = impl_->event_actor_ptr_;

    popUid(*impl_->actor_dq_.back().actor_ptr_);

    impl_->event_actor_ptr_->registerCompletionHandlers();

    impl_->running_ = true;

    return true;
}

/*virtual*/ void Reactor::stop()
{
    solid_log(frame_logger, Verbose, "");
    lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->must_stop_ = true;
    impl_->cnd_var_.notify_one();
}

void Reactor::run()
{
    solid_log(frame_logger, Verbose, "<enter>");
    bool running         = true;
    impl_->current_time_ = NanoTime::nowSteady();
    while (running) {

        crtload = actor_count_ + current_exec_size_;

        if (doWaitEvent(impl_->current_time_, current_exec_size_ == 0)) {
            impl_->current_time_ = NanoTime::nowSteady();
            doCompleteEvents(impl_->current_time_, impl_->dummyCompletionHandlerUid());
        }
        impl_->current_time_ = NanoTime::nowSteady();
        doCompleteTimer(impl_->current_time_);

        impl_->current_time_ = NanoTime::nowSteady();
        doCompleteExec(impl_->current_time_);

        running = impl_->running_ || (actor_count_ != 0) || current_exec_size_ != 0;
    }
    impl_->event_actor_ptr_->stop();
    doClearSpecific();
    solid_log(frame_logger, Verbose, "<exit>");
}

UniqueId Reactor::actorUid(ReactorContext const& _rctx) const
{
    return UniqueId(_rctx.actor_idx_, impl_->actor_dq_[_rctx.actor_idx_].unique_);
}

Service& Reactor::service(ReactorContext const& _rctx) const
{
    return *impl_->actor_dq_[_rctx.actor_idx_].pservice_;
}

Actor& Reactor::actor(ReactorContext const& _rctx) const
{
    return *impl_->actor_dq_[_rctx.actor_idx_].actor_ptr_;
}

CompletionHandler* Reactor::completionHandler(ReactorContext const& _rctx) const
{
    return impl_->completion_handler_dq_[_rctx.completion_heandler_idx_].pcompletion_handler_;
}

void Reactor::addActor(UniqueId const& _uid, Service& _rservice, ActorPointerT&& _actor_ptr)
{
    if (_uid.index >= impl_->actor_dq_.size()) {
        impl_->actor_dq_.resize(static_cast<size_t>(_uid.index + 1));
    }

    ActorStub& rstub = impl_->actor_dq_[static_cast<size_t>(_uid.index)];
    solid_assert_log(rstub.unique_ == _uid.unique, frame_logger);

    {
        // NOTE: we must lock the mutex of the actor
        // in order to ensure that actor is fully registered onto the manager

        lock_guard<Service::ActorMutexT> lock(_rservice.mutex(*_actor_ptr));
    }

    rstub.actor_ptr_ = std::move(_actor_ptr);
    rstub.pservice_  = &_rservice;
    rstub.actor_ptr_->registerCompletionHandlers();
}

bool Reactor::isValid(UniqueId const& _actor_uid, UniqueId const& _completion_handler_uid) const
{
    ActorStub&             ras(impl_->actor_dq_[static_cast<size_t>(_actor_uid.index)]);
    CompletionHandlerStub& rcs(impl_->completion_handler_dq_[static_cast<size_t>(_completion_handler_uid.index)]);
    return ras.unique_ == _actor_uid.unique && rcs.unique_ == _completion_handler_uid.unique;
}

void Reactor::postActorStop(ReactorContext& _rctx)
{
    doPostActorStop(_rctx, impl_->dummyCompletionHandlerUid());
}

/*static*/ void Reactor::stop_actor_repost(ReactorContext& _rctx, EventBase&& /*_uevent*/)
{
    Reactor& rthis = _rctx.reactor();
    rthis.doStopActorRepost(_rctx, rthis.impl_->dummyCompletionHandlerUid());
}

/*static*/ void Reactor::stop_actor(ReactorContext& _rctx, EventBase&&)
{
    Reactor& rthis = _rctx.reactor();
    rthis.doStopActor(_rctx);
}

void Reactor::doStopActor(ReactorContext& _rctx)
{
    ActorStub& rstub = impl_->actor_dq_[_rctx.actor_idx_];

    this->stopActor(*rstub.actor_ptr_, rstub.pservice_->manager());

    rstub.clear();
    --actor_count_;
    impl_->freeuid_vec_.push_back(UniqueId(_rctx.actor_idx_, rstub.unique_));
}

void Reactor::onTimer(ReactorContext& _rctx, const size_t _chidx)
{
    CompletionHandlerStub& rch = impl_->completion_handler_dq_[_chidx];

    _rctx.reactor_event_           = ReactorEventE::Timer;
    _rctx.completion_heandler_idx_ = _chidx;
    _rctx.actor_idx_               = rch.actor_idx_;

    rch.pcompletion_handler_->handleCompletion(_rctx);
    _rctx.clearError();
}

void Reactor::doCompleteTimer(NanoTime const& _rcrttime)
{
    ReactorContext ctx(*this, _rcrttime);
    impl_->time_store_.pop(_rcrttime, [this, &ctx](const size_t _chidx, const NanoTime& /* _expiry */, const size_t /* _proxy_index */) {
        onTimer(ctx, _chidx);
    });
}

bool Reactor::doWaitEvent(NanoTime const& _rcrttime, const bool _exec_q_empty)
{
    bool                    rv            = false;
    auto                    wait_duration = impl_->computeWaitDuration(_rcrttime, _exec_q_empty);
    unique_lock<std::mutex> lock(impl_->mutex_);

    if (current_push_size_ == 0u && current_wake_size_ == 0u && !impl_->must_stop_) {
        if (wait_duration) {
            const auto nanosecs = wait_duration.durationCast<std::chrono::nanoseconds>();
            impl_->cnd_var_.wait_for(lock, nanosecs);
        } else if (wait_duration == NanoTime::max()) {
            impl_->cnd_var_.wait(lock);
        }
    }

    if (impl_->must_stop_) {
        impl_->running_   = false;
        impl_->must_stop_ = false;
    }

    if (current_push_size_ != 0u) {
        current_push_size_ = 0;
        for (auto& v : impl_->freeuid_vec_) {
            this->pushUid(v);
        }
        impl_->freeuid_vec_.clear();

        const size_t current_push_index = current_push_index_;
        current_push_index_             = ((current_push_index + 1) & 1);
        rv                              = true;
    }
    if (current_wake_size_ != 0u) {
        current_wake_size_              = 0;
        const size_t current_push_index = current_wake_index_;
        current_wake_index_             = ((current_push_index + 1) & 1);
        rv                              = true;
    }
    return rv;
}

UniqueId Reactor::popUid(Actor& _ractor)
{
    return ReactorBase::popUid(_ractor);
}

/*static*/ void Reactor::call_actor_on_event(ReactorContext& _rctx, EventBase&& _uevent)
{
    _rctx.actor().onEvent(_rctx, std::move(_uevent));
}

bool Reactor::addTimer(CompletionHandler const& _rch, NanoTime const& _rt, size_t& _rstoreidx)
{
    if (_rstoreidx != InvalidIndex()) {
        impl_->time_store_.update(_rstoreidx, impl_->current_time_, _rt);
    } else {
        _rstoreidx = impl_->time_store_.push(impl_->current_time_, _rt, _rch.idxreactor);
    }
    return true;
}

bool Reactor::remTimer(CompletionHandler const& /*_rch*/, size_t const& _rstoreidx)
{
    if (_rstoreidx != InvalidIndex()) {
        impl_->time_store_.pop(_rstoreidx);
    }
    return true;
}

void Reactor::registerCompletionHandler(CompletionHandler& _rch, Actor const& _ract)
{
    solid_log(frame_logger, Verbose, "");
    size_t idx;
    if (!impl_->completion_handler_index_stk_.empty()) {
        idx = impl_->completion_handler_index_stk_.top();
        impl_->completion_handler_index_stk_.pop();
    } else {
        idx = impl_->completion_handler_dq_.size();
        impl_->completion_handler_dq_.push_back(CompletionHandlerStub());
    }

    CompletionHandlerStub& rcs = impl_->completion_handler_dq_[idx];

    rcs.actor_idx_           = static_cast<size_t>(_ract.ActorBase::runId().index);
    rcs.pcompletion_handler_ = &_rch;
    _rch.idxreactor          = idx;
    {
        NanoTime       dummytime;
        ReactorContext ctx(*this, dummytime);

        ctx.reactor_event_           = ReactorEventE::Init;
        ctx.actor_idx_               = rcs.actor_idx_;
        ctx.completion_heandler_idx_ = idx;

        _rch.handleCompletion(ctx);
    }
}

void Reactor::unregisterCompletionHandler(CompletionHandler& _rch)
{
    solid_log(frame_logger, Verbose, "");
    CompletionHandlerStub& rcs = impl_->completion_handler_dq_[_rch.idxreactor];
    {
        NanoTime       dummytime;
        ReactorContext ctx(*this, dummytime);

        ctx.reactor_event_           = ReactorEventE::Clear;
        ctx.actor_idx_               = rcs.actor_idx_;
        ctx.completion_heandler_idx_ = _rch.idxreactor;

        _rch.handleCompletion(ctx);
    }

    rcs.pcompletion_handler_ = &impl_->event_actor_ptr_->dummy_handler_;
    rcs.actor_idx_           = 0;
    ++rcs.unique_;
}

thread_local Reactor* thread_local_reactor = nullptr;

/*static*/ Reactor* Reactor::safeSpecific()
{
    return thread_local_reactor;
}

void Reactor::doStoreSpecific()
{
    thread_local_reactor = this;
}
void Reactor::doClearSpecific()
{
    thread_local_reactor = nullptr;
}

/*static*/ Reactor& Reactor::specific()
{
    solid_log(frame_logger, Verbose, "");
    return *safeSpecific();
}

} // namespace impl

#ifdef false
namespace {

void dummy_completion(CompletionHandler&, ReactorContext&)
{
}

} // namespace

typedef std::atomic<bool>   AtomicBoolT;
typedef std::atomic<size_t> AtomicSizeT;
typedef Reactor::TaskT      TaskT;

class EventActor : public Actor {
public:
    EventActor()
        : dummyhandler(proxy(), dummy_completion)
    {
    }

    void stop()
    {
        dummyhandler.deactivate();
        dummyhandler.unregister();
    }

    template <class F>
    void post(ReactorContext& _rctx, F _f)
    {
        Actor::post(_rctx, _f);
    }

    CompletionHandler dummyhandler;
};

struct NewTaskStub {
    NewTaskStub(
        UniqueId const& _ruid, TaskT&& _ractptr, Service& _rsvc, Event const& _revent)
        : uid(_ruid)
        , actptr(std::move(_ractptr))
        , rsvc(_rsvc)
        , event(_revent)
    {
    }

    // NewTaskStub(){}
    UniqueId uid;
    TaskT    actptr;
    Service& rsvc;
    Event    event;
};

struct RaiseEventStub {
    RaiseEventStub(
        UniqueId const& _ruid, Event const& _revent)
        : uid(_ruid)
        , event(_revent)
    {
    }

    RaiseEventStub(
        UniqueId const& _ruid, Event&& _uevent)
        : uid(_ruid)
        , event(std::move(_uevent))
    {
    }

    RaiseEventStub(const RaiseEventStub&) = delete;

    RaiseEventStub(
        RaiseEventStub&& _ures) noexcept
        : uid(_ures.uid)
        , event(std::move(_ures.event))
    {
    }

    UniqueId uid;
    Event    event;
};

struct CompletionHandlerStub {
    CompletionHandlerStub(
        CompletionHandler* _pch    = nullptr,
        const size_t       _actidx = InvalidIndex())
        : pch(_pch)
        , actidx(_actidx)
        , unique(0)
    {
    }

    CompletionHandler* pch;
    size_t             actidx;
    UniqueT            unique;
};

struct ActorStub {
    ActorStub()
        : unique(0)
        , psvc(nullptr)
    {
    }

    UniqueT  unique;
    Service* psvc;
    TaskT    actptr;
};

enum {
    MinEventCapacity = 32,
    MaxEventCapacity = 1024 * 64
};

struct ExecStub {
    template <class F>
    ExecStub(
        UniqueId const& _ruid, F _f, Event&& _uevent = Event())
        : actuid(_ruid)
        , exefnc(_f)
        , event(std::move(_uevent))
    {
    }

    template <class F>
    ExecStub(
        UniqueId const& _ruid, F _f, UniqueId const& _rchnuid, Event&& _revent = Event())
        : actuid(_ruid)
        , chnuid(_rchnuid)
        , exefnc(std::move(_f))
        , event(std::move(_revent))
    {
    }

    ExecStub(
        UniqueId const& _ruid, Event&& _revent)
        : actuid(_ruid)
        , event(std::move(_revent))
    {
    }

    ExecStub(
        UniqueId const& _ruid, Event const& _revent = Event())
        : actuid(_ruid)
        , event(_revent)
    {
    }

    ExecStub(
        ExecStub&& _res) noexcept
        : actuid(_res.actuid)
        , chnuid(_res.chnuid)
        , event(std::move(_res.event))
    {
        std::swap(exefnc, _res.exefnc);
    }

    UniqueId                actuid;
    UniqueId                chnuid;
    Reactor::EventFunctionT exefnc;
    Event                   event;
};

typedef std::vector<NewTaskStub>          NewTaskVectorT;
typedef std::vector<RaiseEventStub>       RaiseEventVectorT;
typedef std::deque<CompletionHandlerStub> CompletionHandlerDequeT;
typedef std::vector<UniqueId>             UidVectorT;
typedef std::deque<ActorStub>             ActorDequeT;
typedef Queue<ExecStub>                   ExecQueueT;
typedef Stack<size_t>                     SizeStackT;
typedef TimeStore                         TimeStoreT;

struct Reactor::Data {
    Data(

        )
        : running(false)
        , must_stop(false)
        , crtpushtskvecidx(0)
        , crtraisevecidx(0)
        , crtpushvecsz(0)
        , crtraisevecsz(0)
        , actcnt(0)
        , time_store_(MinEventCapacity)
        , event_actor_ptr(make_shared<EventActor>())
    {
        pcrtpushtskvec = &pushtskvec[1];
        pcrtraisevec   = &raisevec[1];
    }

    int computeWaitTimeMilliseconds(NanoTime const& _rcrt) const
    {
        if (!exeq.empty()) {
            return 0;
        }

        if (!time_store_.empty()) {
            if (_rcrt < time_store_.expiry()) {
                const int64_t maxwait = 1000 * 60; // 1 minute
                int64_t       diff    = 0;
                //                 NanoTime    delta = timestore.next();
                //                 delta -= _rcrt;
                //                 diff = (delta.seconds() * 1000);
                //                 diff += (delta.nanoSeconds() / 1000000);
                const auto crt_tp  = _rcrt.timePointCast<std::chrono::steady_clock::time_point>();
                const auto next_tp = time_store_.expiry().timePointCast<std::chrono::steady_clock::time_point>();
                diff               = std::chrono::duration_cast<std::chrono::milliseconds>(next_tp - crt_tp).count();
                if (diff > maxwait) {
                    return maxwait;
                }
                return static_cast<int>(diff);
            }

            return 0;
        }
        return -1;
    }

    UniqueId dummyCompletionHandlerUid() const
    {
        const size_t idx = event_actor_ptr->dummyhandler.idxreactor;
        return UniqueId(idx, chdq[idx].unique);
    }

    bool                    running;
    bool                    must_stop;
    size_t                  crtpushtskvecidx;
    size_t                  crtraisevecidx;
    size_t                  crtpushvecsz;
    size_t                  crtraisevecsz;
    size_t                  actcnt;
    TimeStoreT              time_store_;
    NanoTime                current_time_;
    NewTaskVectorT*         pcrtpushtskvec;
    RaiseEventVectorT*      pcrtraisevec;
    mutex                   mtx;
    condition_variable      cnd;
    NewTaskVectorT          pushtskvec[2];
    RaiseEventVectorT       raisevec[2];
    shared_ptr<EventActor>  event_actor_ptr;
    CompletionHandlerDequeT chdq;
    UidVectorT              freeuidvec;
    ActorDequeT             actdq;
    ExecQueueT              exeq;
    SizeStackT              chposcache;
};

Reactor::Reactor(
    SchedulerBase& _rsched,
    const size_t   _idx)
    : ReactorBase(_rsched, _idx)
    , impl_(make_pimpl<Data>())
{
    solid_log(frame_logger, Verbose, "");
}

Reactor::~Reactor()
{
    solid_log(frame_logger, Verbose, "");
}

bool Reactor::start()
{
    doStoreSpecific();
    solid_log(frame_logger, Verbose, "");

    impl_->actdq.push_back(ActorStub());
    impl_->actdq.back().actptr = impl_->event_actor_ptr;

    popUid(*impl_->actdq.back().actptr);

    impl_->event_actor_ptr->registerCompletionHandlers();

    impl_->running = true;

    return true;
}

/*virtual*/ bool Reactor::raise(UniqueId const& _ractuid, Event const& _revent)
{
    solid_log(frame_logger, Verbose, (void*)this << " uid = " << _ractuid.index << ',' << _ractuid.unique << " event = " << _revent);
    bool   rv         = true;
    size_t raisevecsz = 0;
    {
        lock_guard<mutex> lock(impl_->mtx);

        impl_->raisevec[impl_->crtraisevecidx].push_back(RaiseEventStub(_ractuid, _revent));
        raisevecsz           = impl_->raisevec[impl_->crtraisevecidx].size();
        impl_->crtraisevecsz = raisevecsz;
        if (raisevecsz == 1) {
            impl_->cnd.notify_one();
        }
    }
    return rv;
}

/*virtual*/ bool Reactor::raise(UniqueId const& _ractuid, Event&& _uevent)
{
    solid_log(frame_logger, Verbose, (void*)this << " uid = " << _ractuid.index << ',' << _ractuid.unique << " event = " << _uevent);
    bool   rv         = true;
    size_t raisevecsz = 0;
    {
        lock_guard<mutex> lock(impl_->mtx);

        impl_->raisevec[impl_->crtraisevecidx].push_back(RaiseEventStub(_ractuid, std::move(_uevent)));
        raisevecsz           = impl_->raisevec[impl_->crtraisevecidx].size();
        impl_->crtraisevecsz = raisevecsz;
        if (raisevecsz == 1) {
            impl_->cnd.notify_one();
        }
    }
    return rv;
}

/*virtual*/ void Reactor::stop()
{
    solid_log(frame_logger, Verbose, "");
    lock_guard<mutex> lock(impl_->mtx);
    impl_->must_stop = true;
    impl_->cnd.notify_one();
}

// Called from outside reactor's thread
bool Reactor::push(TaskT&& _ract, Service& _rsvc, Event const& _revent)
{
    solid_log(frame_logger, Verbose, (void*)this);
    bool   rv        = true;
    size_t pushvecsz = 0;
    {
        lock_guard<mutex> lock(impl_->mtx);
        const UniqueId    uid = this->popUid(*_ract);

        solid_log(frame_logger, Verbose, (void*)this << " uid = " << uid.index << ',' << uid.unique << " event = " << _revent);

        impl_->pushtskvec[impl_->crtpushtskvecidx].push_back(NewTaskStub(uid, std::move(_ract), _rsvc, _revent));
        pushvecsz           = impl_->pushtskvec[impl_->crtpushtskvecidx].size();
        impl_->crtpushvecsz = pushvecsz;
        if (pushvecsz == 1) {
            impl_->cnd.notify_one();
        }
    }

    return rv;
}

void Reactor::run()
{
    solid_log(frame_logger, Verbose, "<enter>");
    bool running         = true;
    impl_->current_time_ = NanoTime::nowSteady();
    while (running) {

        crtload = impl_->actcnt + impl_->exeq.size();

        if (doWaitEvent(impl_->current_time_)) {
            impl_->current_time_ = NanoTime::nowSteady();
            doCompleteEvents(impl_->current_time_);
        }
        impl_->current_time_ = NanoTime::nowSteady();
        doCompleteTimer(impl_->current_time_);

        impl_->current_time_ = NanoTime::nowSteady();
        doCompleteExec(impl_->current_time_);

        running = impl_->running || (impl_->actcnt != 0) || !impl_->exeq.empty();
    }
    impl_->event_actor_ptr->stop();
    doClearSpecific();
    solid_log(frame_logger, Verbose, "<exit>");
}

UniqueId Reactor::actorUid(ReactorContext const& _rctx) const
{
    return UniqueId(_rctx.actidx, impl_->actdq[_rctx.actidx].unique);
}

Service& Reactor::service(ReactorContext const& _rctx) const
{
    return *impl_->actdq[_rctx.actidx].psvc;
}

Actor& Reactor::actor(ReactorContext const& _rctx) const
{
    return *impl_->actdq[_rctx.actidx].actptr;
}

CompletionHandler* Reactor::completionHandler(ReactorContext const& _rctx) const
{
    return impl_->chdq[_rctx.chnidx].pch;
}

void Reactor::doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, Event&& _uev)
{
    impl_->exeq.push(ExecStub(_rctx.actorUid(), std::move(_uev)));
    impl_->exeq.back().exefnc = std::move(_revfn);
    impl_->exeq.back().chnuid = impl_->dummyCompletionHandlerUid();
}

void Reactor::doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, Event&& _uev, CompletionHandler const& _rch)
{
    impl_->exeq.push(ExecStub(_rctx.actorUid(), std::move(_uev)));
    impl_->exeq.back().exefnc = std::move(_revfn);
    impl_->exeq.back().chnuid = UniqueId(_rch.idxreactor, impl_->chdq[_rch.idxreactor].unique);
}

/*static*/ void Reactor::stop_actor_repost(ReactorContext& _rctx, Event&& /*_uevent*/)
{
    Reactor& rthis = _rctx.reactor();
    rthis.impl_->exeq.push(ExecStub(_rctx.actorUid()));
    rthis.impl_->exeq.back().exefnc = &stop_actor;
    rthis.impl_->exeq.back().chnuid = rthis.impl_->dummyCompletionHandlerUid();
}

/*static*/ void Reactor::stop_actor(ReactorContext& _rctx, Event&&)
{
    Reactor& rthis = _rctx.reactor();
    rthis.doStopActor(_rctx);
}

/*NOTE:
    We do not stop the actor rightaway - we make sure that any
    pending Events are delivered to the actor before we stop
*/
void Reactor::postActorStop(ReactorContext& _rctx)
{
    impl_->exeq.push(ExecStub(_rctx.actorUid()));
    impl_->exeq.back().exefnc = &stop_actor_repost;
    impl_->exeq.back().chnuid = impl_->dummyCompletionHandlerUid();
}

void Reactor::doStopActor(ReactorContext& _rctx)
{
    ActorStub& ros = this->impl_->actdq[_rctx.actidx];

    this->stopActor(*ros.actptr, ros.psvc->manager());

    ros.actptr.reset();
    ros.psvc = nullptr;
    ++ros.unique;
    --this->impl_->actcnt;
    this->impl_->freeuidvec.push_back(UniqueId(_rctx.actidx, ros.unique));
}

void Reactor::onTimer(ReactorContext& _rctx, const size_t _chidx)
{
    CompletionHandlerStub& rch = impl_->chdq[_chidx];

    _rctx.reactevn = ReactorEventTimer;
    _rctx.chnidx   = _chidx;
    _rctx.actidx   = rch.actidx;

    rch.pch->handleCompletion(_rctx);
    _rctx.clearError();
}

void Reactor::doCompleteTimer(NanoTime const& _rcrttime)
{
    ReactorContext ctx(*this, _rcrttime);
    impl_->time_store_.pop(_rcrttime, [this, &ctx](const size_t _chidx, const NanoTime& /* _expiry */, const size_t /* _proxy_index */) {
        onTimer(ctx, _chidx);
    });
}

void Reactor::doCompleteExec(NanoTime const& _rcrttime)
{
    ReactorContext ctx(*this, _rcrttime);
    size_t         sz = impl_->exeq.size();
    while ((sz--) != 0) {
        ExecStub&              rexe(impl_->exeq.front());
        ActorStub&             ros(impl_->actdq[static_cast<size_t>(rexe.actuid.index)]);
        CompletionHandlerStub& rcs(impl_->chdq[static_cast<size_t>(rexe.chnuid.index)]);

        if (ros.unique == rexe.actuid.unique && rcs.unique == rexe.chnuid.unique) {
            ctx.clearError();
            ctx.chnidx = static_cast<size_t>(rexe.chnuid.index);
            ctx.actidx = static_cast<size_t>(rexe.actuid.index);
            rexe.exefnc(ctx, std::move(rexe.event));
        }
        impl_->exeq.pop();
    }
}

bool Reactor::doWaitEvent(NanoTime const& _rcrttime)
{
    bool               rv       = false;
    int                waitmsec = impl_->computeWaitTimeMilliseconds(_rcrttime);
    unique_lock<mutex> lock(impl_->mtx);

    if (impl_->crtpushvecsz == 0 && impl_->crtraisevecsz == 0 && !impl_->must_stop) {
        if (waitmsec > 0) {
            impl_->cnd.wait_for(lock, std::chrono::milliseconds(waitmsec));
        } else if (waitmsec < 0) {
            impl_->cnd.wait(lock);
        }
    }

    if (impl_->must_stop) {
        impl_->running   = false;
        impl_->must_stop = false;
    }
    if (impl_->crtpushvecsz != 0u) {
        impl_->crtpushvecsz = 0;
        for (auto& v : impl_->freeuidvec) {
            this->pushUid(v);
        }
        impl_->freeuidvec.clear();

        const size_t crtpushvecidx = impl_->crtpushtskvecidx;
        impl_->crtpushtskvecidx    = ((crtpushvecidx + 1) & 1);
        impl_->pcrtpushtskvec      = &impl_->pushtskvec[crtpushvecidx];
        rv                         = true;
    }
    if (impl_->crtraisevecsz != 0u) {
        impl_->crtraisevecsz        = 0;
        const size_t crtraisevecidx = impl_->crtraisevecidx;
        impl_->crtraisevecidx       = ((crtraisevecidx + 1) & 1);
        impl_->pcrtraisevec         = &impl_->raisevec[crtraisevecidx];
        rv                          = true;
    }
    return rv;
}

void Reactor::doCompleteEvents(NanoTime const& _rcrttime)
{
    solid_log(frame_logger, Verbose, "");

    NewTaskVectorT&    crtpushvec  = *impl_->pcrtpushtskvec;
    RaiseEventVectorT& crtraisevec = *impl_->pcrtraisevec;
    ReactorContext     ctx(*this, _rcrttime);

    if (!crtpushvec.empty()) {

        impl_->actcnt += crtpushvec.size();

        for (auto& rnewact : crtpushvec) {

            if (rnewact.uid.index >= impl_->actdq.size()) {
                impl_->actdq.resize(static_cast<size_t>(rnewact.uid.index + 1));
            }

            ActorStub& ros = impl_->actdq[static_cast<size_t>(rnewact.uid.index)];
            solid_assert_log(ros.unique == rnewact.uid.unique, frame_logger);

            {
                // NOTE: we must lock the mutex of the actor
                // in order to ensure that actor is fully registered onto the manager

                lock_guard<std::mutex> lock(rnewact.rsvc.mutex(*rnewact.actptr));
            }

            ros.actptr = std::move(rnewact.actptr);
            ros.psvc   = &rnewact.rsvc;

            ctx.clearError();
            ctx.chnidx = InvalidIndex();
            ctx.actidx = static_cast<size_t>(rnewact.uid.index);

            ros.actptr->registerCompletionHandlers();

            impl_->exeq.push(ExecStub(rnewact.uid, &call_actor_on_event, impl_->dummyCompletionHandlerUid(), std::move(rnewact.event)));
        }
        crtpushvec.clear();
    }

    if (!crtraisevec.empty()) {
        for (auto& revent : crtraisevec) {
            impl_->exeq.push(ExecStub(revent.uid, &call_actor_on_event, impl_->dummyCompletionHandlerUid(), std::move(revent.event)));
        }
        crtraisevec.clear();
    }
}

/*static*/ void Reactor::call_actor_on_event(ReactorContext& _rctx, Event&& _uevent)
{
    _rctx.actor().onEvent(_rctx, std::move(_uevent));
}

bool Reactor::addTimer(CompletionHandler const& _rch, NanoTime const& _rt, size_t& _rstoreidx)
{
    if (_rstoreidx != InvalidIndex()) {
        impl_->time_store_.update(_rstoreidx, impl_->current_time_, _rt);
    } else {
        _rstoreidx = impl_->time_store_.push(impl_->current_time_, _rt, _rch.idxreactor);
    }
    return true;
}

bool Reactor::remTimer(CompletionHandler const& /*_rch*/, size_t const& _rstoreidx)
{
    if (_rstoreidx != InvalidIndex()) {
        impl_->time_store_.pop(_rstoreidx);
    }
    return true;
}

void Reactor::registerCompletionHandler(CompletionHandler& _rch, Actor const& _ract)
{
    solid_log(frame_logger, Verbose, "");
    size_t idx;
    if (!impl_->chposcache.empty()) {
        idx = impl_->chposcache.top();
        impl_->chposcache.pop();
    } else {
        idx = impl_->chdq.size();
        impl_->chdq.push_back(CompletionHandlerStub());
    }
    CompletionHandlerStub& rcs = impl_->chdq[idx];

    rcs.actidx      = static_cast<size_t>(_ract.ActorBase::runId().index);
    rcs.pch         = &_rch;
    _rch.idxreactor = idx;
    {
        NanoTime       dummytime;
        ReactorContext ctx(*this, dummytime);

        ctx.reactevn = ReactorEventInit;
        ctx.actidx   = rcs.actidx;
        ctx.chnidx   = idx;

        _rch.handleCompletion(ctx);
    }
}

void Reactor::unregisterCompletionHandler(CompletionHandler& _rch)
{
    solid_log(frame_logger, Verbose, "");
    CompletionHandlerStub& rcs = impl_->chdq[_rch.idxreactor];
    {
        NanoTime       dummytime;
        ReactorContext ctx(*this, dummytime);

        ctx.reactevn = ReactorEventClear;
        ctx.actidx   = rcs.actidx;
        ctx.chnidx   = _rch.idxreactor;

        _rch.handleCompletion(ctx);
    }

    rcs.pch    = &impl_->event_actor_ptr->dummyhandler;
    rcs.actidx = 0;
    ++rcs.unique;
}

thread_local Reactor* thread_local_reactor = nullptr;

/*static*/ Reactor* Reactor::safeSpecific()
{
    return thread_local_reactor;
}

void Reactor::doStoreSpecific()
{
    thread_local_reactor = this;
}
void Reactor::doClearSpecific()
{
    thread_local_reactor = nullptr;
}

/*static*/ Reactor& Reactor::specific()
{
    solid_log(frame_logger, Verbose, "");
    return *safeSpecific();
}
#endif
//=============================================================================
//      ReactorContext
//=============================================================================

Actor& ReactorContext::actor() const
{
    return reactor().actor(*this);
}

//-----------------------------------------------------------------------------

Service& ReactorContext::service() const
{
    return reactor().service(*this);
}

//-----------------------------------------------------------------------------

Manager& ReactorContext::manager() const
{
    return service().manager();
}

//-----------------------------------------------------------------------------

ActorIdT ReactorContext::actorId() const
{
    return service().id(actor());
}

//-----------------------------------------------------------------------------

UniqueId ReactorContext::actorUid() const
{
    return reactor().actorUid(*this);
}

//-----------------------------------------------------------------------------

Service::ActorMutexT& ReactorContext::actorMutex() const
{
    return service().mutex(actor());
}

//-----------------------------------------------------------------------------

CompletionHandler* ReactorContext::completionHandler() const
{
    return reactor().completionHandler(*this);
}

//=============================================================================
//      ReactorBase
//=============================================================================
UniqueId ReactorBase::popUid(ActorBase& _ract)
{
    UniqueId rv(crtidx, 0);
    if (!uidstk.empty()) {
        rv = uidstk.top();
        uidstk.pop();
    } else {
        ++crtidx;
    }
    _ract.runId(rv);
    return rv;
}

bool ReactorBase::prepareThread(const bool _success)
{
    return scheduler().prepareThread(idInScheduler(), *this, _success);
}
void ReactorBase::unprepareThread()
{
    scheduler().unprepareThread(idInScheduler(), *this);
}

ReactorBase::~ReactorBase() {}

} // namespace frame
} // namespace solid
