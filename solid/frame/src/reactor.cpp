// solid/frame/src/reactor.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <bit>
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
    shared_ptr<EventActor>  event_actor_ptr_ = make_shared<EventActor>();
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
    SchedulerBase& _rsched, StatisticT& _rstatistic,
    const size_t _idx, const size_t _wake_capacity)
    : ReactorBase(_rsched, _idx)
    , wake_capacity_(std::bit_ceil(_wake_capacity))
    , rstatistic_(_rstatistic)
{
    solid_log(frame_logger, Verbose, "");
}

Reactor::~Reactor()
{
    solid_log(frame_logger, Verbose, "");
}

std::mutex& Reactor::mutex()
{
    return impl_->mutex_;
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

void Reactor::notifyOne()
{
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
    return UniqueId(_rctx.actor_index_, impl_->actor_dq_[_rctx.actor_index_].unique_);
}

Service& Reactor::service(ReactorContext const& _rctx) const
{
    return *impl_->actor_dq_[_rctx.actor_index_].pservice_;
}

Actor& Reactor::actor(ReactorContext const& _rctx) const
{
    return *impl_->actor_dq_[_rctx.actor_index_].actor_ptr_;
}

CompletionHandler* Reactor::completionHandler(ReactorContext const& _rctx) const
{
    return impl_->completion_handler_dq_[_rctx.completion_heandler_index_].pcompletion_handler_;
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
    const ActorStub&             ras(impl_->actor_dq_[static_cast<size_t>(_actor_uid.index)]);
    const CompletionHandlerStub& rcs(impl_->completion_handler_dq_[static_cast<size_t>(_completion_handler_uid.index)]);
    solid_assert(ras.actor_ptr_);
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

void Reactor::doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, EventBase&& _uev)
{
    doPost(_rctx, std::move(_revfn), std::move(_uev), impl_->dummyCompletionHandlerUid());
}
void Reactor::doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, EventBase&& _uev, CompletionHandler const& _rch)
{
    doPost(_rctx, std::move(_revfn), std::move(_uev), UniqueId(_rch.idxreactor, impl_->completion_handler_dq_[_rch.idxreactor].unique_));
}

void Reactor::doStopActor(ReactorContext& _rctx)
{
    ActorStub& rstub = impl_->actor_dq_[_rctx.actor_index_];

    this->stopActor(*rstub.actor_ptr_, rstub.pservice_->manager());

    rstub.clear();
    --actor_count_;
    rstatistic_.actorCount(actor_count_);
    impl_->freeuid_vec_.push_back(UniqueId(_rctx.actor_index_, rstub.unique_));
}

void Reactor::onTimer(ReactorContext& _rctx, const size_t _chidx)
{
    CompletionHandlerStub& rch = impl_->completion_handler_dq_[_chidx];

    _rctx.reactor_event_             = ReactorEventE::Timer;
    _rctx.completion_heandler_index_ = _chidx;
    _rctx.actor_index_               = rch.actor_idx_;

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

    if (pending_wake_count_.load() == 0u && !impl_->must_stop_) {
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

    if (pending_wake_count_.load() != 0u) {
        for (auto& v : impl_->freeuid_vec_) {
            this->pushUid(v);
        }
        impl_->freeuid_vec_.clear();
        rv = true;
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

        ctx.reactor_event_             = ReactorEventE::Init;
        ctx.actor_index_               = rcs.actor_idx_;
        ctx.completion_heandler_index_ = idx;

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

        ctx.reactor_event_             = ReactorEventE::Clear;
        ctx.actor_index_               = rcs.actor_idx_;
        ctx.completion_heandler_index_ = _rch.idxreactor;

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

//=============================================================================
//      ReactorStatisticBase
//=============================================================================
std::ostream& ReactorStatisticBase::print(std::ostream& _ros) const
{
    _ros << " push_while_wait_lock_count = " << push_while_wait_lock_count_;
    _ros << " push_while_wait_pushing_count = " << push_while_wait_pushing_count_;
    return _ros;
}

void ReactorStatisticBase::clear()
{
    push_while_wait_lock_count_    = 0;
    push_while_wait_pushing_count_ = 0;
}
//=============================================================================
//      ReactorStatistic
//=============================================================================
std::ostream& ReactorStatistic::print(std::ostream& _ros) const
{
    ReactorStatisticBase::print(_ros);
    _ros << " push_notify_count = " << push_notify_count_;
    _ros << " push_count = " << push_count_;
    _ros << " wake_notify_count = " << wake_notify_count_;
    _ros << " wake_count = " << wake_count_;
    _ros << " post_count = " << post_count_;
    _ros << " post_stop_count = " << post_stop_count_;
    _ros << " max_exec_size = " << max_exec_size_;
    _ros << " actor_count = " << actor_count_;
    _ros << " max_actor_count = " << max_actor_count_;
    return _ros;
}

void ReactorStatistic::clear()
{
    ReactorStatisticBase::clear();
    push_notify_count_ = 0;
    push_count_        = 0;
    wake_notify_count_ = 0;
    wake_count_        = 0;
    post_count_        = 0;
    post_stop_count_   = 0;
    max_exec_size_     = 0;
    actor_count_       = 0;
    max_actor_count_   = 0;
}

} // namespace frame
} // namespace solid
