// solid/frame/aio/src/aioreactor_epoll.cpp
//
// Copyright (c) 2015, 2023 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <fcntl.h>

#include "solid/system/common.hpp"

#if defined(SOLID_USE_EPOLL)

#include <sys/epoll.h>
#include <sys/eventfd.h>

#elif defined(SOLID_USE_KQUEUE)

#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>

#elif defined(SOLID_USE_WSAPOLL)

#ifndef NOMINMAX
#define NOMINMAX
#endif

#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#endif

#include <cerrno>
#include <cstring>
#include <deque>
#include <queue>
#include <vector>

#include "solid/system/device.hpp"
#include "solid/system/error.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/system/nanotime.hpp"
#include "solid/system/spinlock.hpp"
#include <mutex>
#include <thread>

#include "solid/utility/event.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"

#include "solid/frame/actor.hpp"
#include "solid/frame/common.hpp"
#include "solid/frame/service.hpp"
#include "solid/frame/timestore.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aiocompletion.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/frame/aio/aiotimer.hpp"

using namespace std;

namespace solid {
namespace frame {
namespace aio {

namespace {

void dummy_completion(CompletionHandler&, ReactorContext&)
{
}
#if defined(SOLID_USE_KQUEUE)
inline void* index_to_void(const size_t _idx)
{
    return reinterpret_cast<void*>(_idx);
}

inline size_t void_to_index(const void* _ptr)
{
    return reinterpret_cast<size_t>(_ptr);
}
#endif

//=============================================================================

struct CompletionHandlerStub {
    CompletionHandler* pcompletion_handler_;
    size_t             actor_idx_;
    UniqueT            unique_ = 0;
#if defined(SOLID_USE_WSAPOLL)
    size_t connect_idx_ = InvalidIndex();
#endif

    CompletionHandlerStub(
        CompletionHandler* _pch    = nullptr,
        const size_t       _actidx = InvalidIndex())
        : pcompletion_handler_(_pch)
        , actor_idx_(_actidx)
    {
    }
};

//=============================================================================

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

//=============================================================================

constexpr size_t min_event_capacity = 32;
constexpr size_t max_event_capacity = 1024 * 64;

//=============================================================================

#if defined(SOLID_USE_EPOLL)

using EventVectorT = std::vector<epoll_event>;

#elif defined(SOLID_USE_KQUEUE)

using EventVectorT = std::vector<struct kevent>;

#elif defined(SOLID_USE_WSAPOLL)
struct PollStub : WSAPOLLFD {
    PollStub()
    {
        clear();
    }
    void clear()
    {
        this->fd      = SocketDevice::invalidDescriptor();
        this->events  = 0;
        this->revents = 0;
    }
};

using EventVectorT = std::vector<PollStub>;

#endif

using CompletionHandlerDequeT = std::deque<CompletionHandlerStub>;
using UidVectorT              = std::vector<UniqueId>;
using ActorDequeT             = std::deque<ActorStub>;
using SizeStackT              = Stack<size_t>;
using TimeStoreT              = TimeStore;
using SizeTVectorT            = std::vector<size_t>;

} // namespace

//=============================================================================
//  EventHandler
//=============================================================================

struct EventHandler : CompletionHandler {
#if defined(SOLID_USE_EPOLL) || defined(SOLID_USE_KQUEUE)
    Device dev_;
#elif defined(SOLID_USE_WSAPOLL)
    SocketDevice dev_;
#endif
public:
    static void on_init(CompletionHandler&, ReactorContext&);
    static void on_completion(CompletionHandler&, ReactorContext&);

    EventHandler(ActorProxy const& _rop)
        : CompletionHandler(_rop, &on_init)
    {
    }

    void write(impl::Reactor& _rreactor);

    bool init();

    auto descriptor() const
    {
        return dev_.descriptor();
    }
};

namespace {
//=============================================================================
//  EventActor
//=============================================================================

struct EventActor : public Actor {
    EventHandler      event_handler_;
    CompletionHandler dummy_handler_;

public:
    EventActor()
        : event_handler_(proxy())
        , dummy_handler_(proxy(), dummy_completion)
    {
    }

    void stop()
    {
        event_handler_.deactivate();
        event_handler_.unregister();
        dummy_handler_.deactivate();
        dummy_handler_.unregister();
    }

    template <class F>
    void post(ReactorContext& _rctx, F _f)
    {
        Actor::post(_rctx, _f);
    }
};
} // namespace

//=============================================================================
//  Reactor::Data
//=============================================================================
struct impl::Reactor::Data {
    int                     reactor_fd_   = -1;
    atomic_bool             running_      = {false};
    size_t                  device_count_ = 0;
    NanoTime                current_time_;
    TimeStoreT              time_store_;
    MutexT                  mutex_;
    EventVectorT            event_vec_;
    shared_ptr<EventActor>  event_actor_ptr_ = make_shared<EventActor>();
    CompletionHandlerDequeT completion_handler_dq_;
    UidVectorT              freeuid_vec_;
    ActorDequeT             actor_dq_;
    SizeStackT              completion_handler_index_stk_;
#if defined(SOLID_USE_WSAPOLL)
    SizeTVectorT connect_vec_;
#endif

#if defined(SOLID_USE_EPOLL2) || defined(SOLID_USE_KQUEUE)
    NanoTime computeWaitDuration(NanoTime const& _rcrt, const bool _can_wait) const
    {

        if (!_can_wait) {
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
#elif defined(SOLID_USE_EPOLL)
    int          computeWaitDuration(NanoTime const& _rcrt, const bool _can_wait) const
    {

        if (!_can_wait) {
            return 0;
        }

        if (!time_store_.empty()) {

            if (_rcrt < time_store_.expiry()) {

                const int64_t maxwait = 1000 * 60 * 10; // ten minutes
                int64_t       diff    = 0;
                const auto    crt_tp  = _rcrt.timePointCast<std::chrono::steady_clock::time_point>();
                const auto    next_tp = time_store_.expiry().timePointCast<std::chrono::steady_clock::time_point>();
                diff                  = std::chrono::duration_cast<std::chrono::milliseconds>(next_tp - crt_tp).count();

                if (diff > maxwait) {
                    return maxwait;
                }
                return static_cast<int>(diff);
            }

            return 0;
        }
        return -1;
    }
#elif defined(SOLID_USE_WSAPOLL)
    int computeWaitDuration(NanoTime const& _rcrt, const bool _can_wait) const
    {
        if (!_can_wait) {
            return 0;
        } else if (!time_store_.empty()) {

            if (_rcrt < time_store_.expiry()) {

                constexpr int64_t maxwait = 1000 * 60 * 10; // ten minutes
                int64_t           diff    = 0;
                const auto        crt_tp  = _rcrt.timePointCast<std::chrono::steady_clock::time_point>();
                const auto        next_tp = time_store_.expiry().timePointCast<std::chrono::steady_clock::time_point>();
                diff                      = std::chrono::duration_cast<std::chrono::milliseconds>(next_tp - crt_tp).count();

                if (diff > maxwait) {
                    return connect_vec_.empty() ? maxwait : 1000; // wait 1 sec when connect opperations are in place
                } else {
                    return connect_vec_.empty() ? static_cast<int>(diff) : 1000;
                }

            } else {
                return 0;
            }

        } else {
            return connect_vec_.empty() ? -1 : 1000;
        }
    }
#endif

    UniqueId dummyCompletionHandlerUid() const
    {
        const size_t idx = event_actor_ptr_->dummy_handler_.idxreactor;
        return UniqueId(idx, completion_handler_dq_[idx].unique_);
    }
};

//=============================================================================
//-----------------------------------------------------------------------------
//  Reactor
//-----------------------------------------------------------------------------
namespace impl {
Reactor::Reactor(
    SchedulerBase& _rsched, StatisticT& _rstatistic,
    const size_t _idx, const size_t _wake_capacity)
    : ReactorBase(_rsched, _idx)
    , wake_capacity_(_wake_capacity)
    , rstatistic_(_rstatistic)
{
    solid_log(logger, Verbose, "");
}

//-----------------------------------------------------------------------------

Reactor::~Reactor()
{
    solid_log(logger, Verbose, "");
}

//-----------------------------------------------------------------------------

bool Reactor::start()
{

    solid_log(logger, Verbose, "");

    doStoreSpecific();

#if defined(SOLID_USE_EPOLL)
    impl_->reactor_fd_ = epoll_create(min_event_capacity);
    if (impl_->reactor_fd_ < 0) {
        solid_log(logger, Error, "reactor create: " << last_system_error().message());
        return false;
    }
#elif defined(SOLID_USE_KQUEUE)
    impl_->reactor_fd_ = kqueue();
    if (impl_->reactor_fd_ < 0) {
        solid_log(logger, Error, "reactor create: " << last_system_error().message());
        return false;
    }
#endif

    if (!impl_->event_actor_ptr_->event_handler_.init()) {
        return false;
    }

    impl_->actor_dq_.push_back(ActorStub());
    impl_->actor_dq_.back().actor_ptr_ = impl_->event_actor_ptr_;

    popUid(*impl_->actor_dq_.back().actor_ptr_);

    impl_->event_actor_ptr_->registerCompletionHandlers();
    impl_->event_vec_.resize(min_event_capacity);
    impl_->event_vec_.resize(impl_->event_vec_.capacity());
    impl_->running_ = true;

    return true;
}

//-----------------------------------------------------------------------------

/*virtual*/ void Reactor::stop()
{
    solid_log(logger, Verbose, "");
    bool expect = true;
    if (impl_->running_.compare_exchange_strong(expect, false)) {
        impl_->event_actor_ptr_->event_handler_.write(*this);
    }
}

//-----------------------------------------------------------------------------

/*NOTE:

    We MUST call doCompleteEvents before doCompleteExec
    because we must ensure that on successful Event notification from
    frame::Manager, the Actor actually receives the Event before stopping.

    For that, on Actor::postStop, we mark the Actor as “unable to
    receive any notifications” (we do not unregister it, because the
    Actor may want access to it’s mutex on events already waiting
    to be delivered to the actor.

*/
// #define SOLID_AIO_TRACE_DURATION
void Reactor::run()
{
    using namespace std::chrono;

    solid_log(logger, Info, "<enter>");
    long     selcnt;
    bool     running = true;
    int      waitmsec;
    NanoTime waittime;
    size_t   waitcnt = 0;

    while (running) {
        impl_->current_time_ = NanoTime::nowSteady();

        crtload = actor_count_ + impl_->device_count_ + current_exec_size_;
#if defined(SOLID_USE_EPOLL2)
        waittime = impl_->computeWaitDuration(impl_->current_time_, current_exec_size_ == 0 && pending_wake_count_.load() == 0);

        solid_log(logger, Verbose, "epoll_wait wait = " << waittime << ' ' << impl_->reactor_fd_ << ' ' << impl_->event_vec_.size());
        selcnt = epoll_pwait2(impl_->reactor_fd_, impl_->event_vec_.data(), static_cast<int>(impl_->event_vec_.size()), waittime != NanoTime::max() ? &waittime : nullptr, nullptr);
        if (waittime.seconds() != 0 && waittime.nanoSeconds() != 0) {
            ++waitcnt;
        }
#elif defined(SOLID_USE_EPOLL)
        waitmsec = impl_->computeWaitDuration(impl_->current_time_, current_exec_size_ == 0 && pending_wake_count_.load() == 0);

        solid_log(logger, Verbose, "epoll_wait wait = " << waitmsec);

        selcnt = epoll_wait(impl_->reactor_fd_, impl_->event_vec_.data(), static_cast<int>(impl_->event_vec_.size()), waitmsec);
#elif defined(SOLID_USE_KQUEUE)
        waittime = impl_->computeWaitDuration(impl_->current_time_, current_exec_size_ == 0 && pending_wake_count_.load() == 0);

        solid_log(logger, Verbose, "kqueue wait = " << waittime);

        selcnt = kevent(impl_->reactor_fd_, nullptr, 0, impl_->event_vec_.data(), static_cast<int>(impl_->event_vec_.size()), waittime != NanoTime::max() ? &waittime : nullptr);
#elif defined(SOLID_USE_WSAPOLL)
        waitmsec = impl_->computeWaitDuration(impl_->current_time_, current_exec_size_ == 0 && pending_wake_count_.load() == 0);
        solid_log(logger, Verbose, "wsapoll wait msec = " << waitmsec);
        selcnt = WSAPoll(impl_->event_vec_.data(), impl_->event_vec_.size(), waitmsec);
#endif
        impl_->current_time_ = NanoTime::nowSteady();
#ifdef SOLID_AIO_TRACE_DURATION
        const auto start = high_resolution_clock::now();
#endif
#if defined(SOLID_USE_WSAPOLL)
        if (selcnt > 0 || impl_->connect_vec_.size()) {
#else
        if (selcnt > 0) {
#endif
            crtload += selcnt;
            doCompleteIo(impl_->current_time_, selcnt);
        } else if (selcnt < 0 && errno != EINTR) {
            solid_log(logger, Error, "epoll_wait errno  = " << last_system_error().message());
            running = false;
            continue;
        } else {
            solid_log(logger, Verbose, "epoll_wait done");
        }
#ifdef SOLID_AIO_TRACE_DURATION
        const auto elapsed_io = duration_cast<microseconds>(high_resolution_clock::now() - start).count();
#endif
        impl_->current_time_ = NanoTime::nowSteady();
        doCompleteTimer(impl_->current_time_);
#ifdef SOLID_AIO_TRACE_DURATION
        const auto elapsed_timer = duration_cast<microseconds>(high_resolution_clock::now() - start).count();
#endif
        impl_->current_time_ = NanoTime::nowSteady();
        doCompleteEvents(impl_->current_time_); // See NOTE above
#ifdef SOLID_AIO_TRACE_DURATION
        const auto elapsed_event = duration_cast<microseconds>(high_resolution_clock::now() - start).count();
#endif
        impl_->current_time_ = NanoTime::nowSteady();
        const auto execnt    = doCompleteExec(impl_->current_time_);
#ifdef SOLID_AIO_TRACE_DURATION
        const auto elapsed_total = duration_cast<microseconds>(high_resolution_clock::now() - start).count();

        if (elapsed_total >= 6000) {
            solid_log(logger, Warning, "reactor loop duration: io " << elapsed_io << " timers " << elapsed_timer << " events " << elapsed_event << " total " << elapsed_total << " execnt " << execnt);
        }
#endif
        (void)execnt;
        running = impl_->running_ || (actor_count_ != 0) || current_exec_size_ != 0;
    }
    solid_log(logger, Warning, "reactor waitcount = " << waitcnt);

    impl_->event_actor_ptr_->stop();
    doClearSpecific();
    solid_log(logger, Info, "<exit>");
    (void)waitmsec;
    (void)waittime;
} // namespace aio

//-----------------------------------------------------------------------------
#if defined(SOLID_USE_EPOLL)
inline ReactorEventE systemEventsToReactorEvents(const uint32_t _events)
{
    ReactorEventE retval = ReactorEventE::None;

    switch (_events) {
    case EPOLLIN:
        retval = ReactorEventE::Recv;
        break;
    case EPOLLOUT:
        retval = ReactorEventE::Send;
        break;
    case EPOLLOUT | EPOLLIN:
        retval = ReactorEventE::RecvSend;
        break;
    case EPOLLPRI:
        retval = ReactorEventE::OOB;
        break;
    case EPOLLOUT | EPOLLPRI:
        retval = ReactorEventE::OOBSend;
        break;
    case EPOLLERR:
        retval = ReactorEventE::Error;
        break;
    case EPOLLHUP:
    case EPOLLHUP | EPOLLOUT:
    case EPOLLHUP | EPOLLIN:
    case EPOLLHUP | EPOLLERR | EPOLLIN | EPOLLOUT:
    case EPOLLIN | EPOLLOUT | EPOLLHUP:
    case EPOLLERR | EPOLLOUT | EPOLLIN:
    case EPOLLERR | EPOLLOUT | EPOLLHUP:
    case EPOLLERR | EPOLLIN | EPOLLHUP:
    case EPOLLERR | EPOLLIN:
    case EPOLLERR | EPOLLOUT:
        retval = ReactorEventE::Hangup;
        break;
#ifdef SOLID_USE_EPOLLRDHUP
    case EPOLLRDHUP:
        retval = ReactorEventE::RecvHangup;
        break;
#endif
    default:
        solid_assert_log(false, logger);
        break;
    }
    return retval;
}

#elif defined(SOLID_USE_KQUEUE)

inline constexpr ReactorEventE systemEventsToReactorEvents(const unsigned short _flags, const short _filter)
{
    ReactorEventE retval = ReactorEventE::None;
    if (_flags & (EV_EOF | EV_ERROR)) {
        return ReactorEventE::Hangup;
    }
    if (_filter == EVFILT_READ) {
        return ReactorEventE::Recv;
    } else if (_filter == EVFILT_WRITE) {
        return ReactorEventE::Send;
    } else if (_filter == EVFILT_USER) {
        return ReactorEventE::Recv;
    }

    return retval;
}
#elif defined(SOLID_USE_WSAPOLL)
inline ReactorEventE systemEventsToReactorEvents(const uint32_t _events, decltype(WSAPOLLFD::events)& _revs)
{
    if (_events & POLLERR) {
        _revs = 0;
        return ReactorEventE::Error;
    }
    if (_events & POLLHUP) {
        _revs = 0;
        return ReactorEventE::Hangup;
    }
    ReactorEventE retval = ReactorEventE::None;
    switch (_events) {
    case POLLPRI:
        retval = ReactorEventE::OOB;
        break;
    case POLLRDNORM:
        retval = ReactorEventE::Recv;
        _revs &= ~(POLLRDNORM);
        break;
    case POLLWRNORM:
        retval = ReactorEventE::Send;
        _revs &= ~(POLLWRNORM);
        break;
    case POLLRDNORM | POLLWRNORM:
        retval = ReactorEventE::RecvSend;
        _revs  = 0;
        break;
    default:
        solid_assert_log(false, logger);
        break;
    }
    return retval;
}
#endif

//-----------------------------------------------------------------------------
#if defined(SOLID_USE_EPOLL)
inline uint32_t reactorRequestsToSystemEvents(const ReactorWaitRequestE _requests)
{
    uint32_t evs = 0;
    switch (_requests) {
    case ReactorWaitRequestE::None:
        break;
    case ReactorWaitRequestE::Read:
        evs = EPOLLET | EPOLLIN;
        break;
    case ReactorWaitRequestE::Write:
        evs = EPOLLET | EPOLLOUT;
        break;
    case ReactorWaitRequestE::ReadOrWrite:
        evs = EPOLLET | EPOLLIN | EPOLLOUT;
        break;
    default:
        solid_assert_log(false, logger);
    }
    return evs;
}
#elif defined(SOLID_USE_WSAPOLL)
inline uint32_t reactorRequestsToSystemEvents(const ReactorWaitRequestE _requests)
{
    uint32_t evs = 0;
    switch (_requests) {
    case ReactorWaitRequestE::None:
        break;
    case ReactorWaitRequestE::Read:
        evs = POLLRDNORM;
        break;
    case ReactorWaitRequestE::Write:
        evs = POLLWRNORM;
        break;
    case ReactorWaitRequestE::ReadOrWrite:
        evs = POLLWRNORM | POLLRDNORM;
        break;
    default:
        solid_assert_log(false, logger);
    }
    return evs;
}
#endif
//-----------------------------------------------------------------------------

UniqueId Reactor::actorUid(ReactorContext const& _rctx) const
{
    return UniqueId(_rctx.actor_index_, impl_->actor_dq_[_rctx.actor_index_].unique_);
}
//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------
bool Reactor::isValid(UniqueId const& _actor_uid, UniqueId const& _completion_handler_uid) const
{
    ActorStub&             ras(impl_->actor_dq_[static_cast<size_t>(_actor_uid.index)]);
    CompletionHandlerStub& rcs(impl_->completion_handler_dq_[static_cast<size_t>(_completion_handler_uid.index)]);
    return ras.unique_ == _actor_uid.unique && rcs.unique_ == _completion_handler_uid.unique;
}
//-----------------------------------------------------------------------------

Service& Reactor::service(ReactorContext const& _rctx) const
{
    return *impl_->actor_dq_[_rctx.actor_index_].pservice_;
}

//-----------------------------------------------------------------------------

Actor& Reactor::actor(ReactorContext const& _rctx) const
{
    return *impl_->actor_dq_[_rctx.actor_index_].actor_ptr_;
}

//-----------------------------------------------------------------------------

CompletionHandler* Reactor::completionHandler(ReactorContext const& _rctx) const
{
    return impl_->completion_handler_dq_[_rctx.completion_heandler_index_].pcompletion_handler_;
}

//-----------------------------------------------------------------------------
void Reactor::doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, EventBase&& _uev)
{
    doPost(_rctx, std::move(_revfn), std::move(_uev), impl_->dummyCompletionHandlerUid());
}
void Reactor::doPost(ReactorContext& _rctx, EventFunctionT&& _revfn, EventBase&& _uev, CompletionHandler const& _rch)
{
    doPost(_rctx, std::move(_revfn), std::move(_uev), UniqueId(_rch.idxreactor, impl_->completion_handler_dq_[_rch.idxreactor].unique_));
}
//-----------------------------------------------------------------------------

void Reactor::pushFreeUids()
{
    for (const auto& v : impl_->freeuid_vec_) {
        this->pushUid(v);
    }
    impl_->freeuid_vec_.clear();
}

bool Reactor::emptyFreeUids() const
{
    return impl_->freeuid_vec_.empty();
}

UniqueId Reactor::popUid(Actor& _ractor)
{
    return ReactorBase::popUid(_ractor);
}
void Reactor::notifyOne()
{
    impl_->event_actor_ptr_->event_handler_.write(*this);
}
Reactor::MutexT& Reactor::mutex()
{
    return impl_->mutex_;
}
//-----------------------------------------------------------------------------

/*static*/ void Reactor::stop_actor(ReactorContext& _rctx, EventBase&&)
{
    Reactor& rthis = _rctx.reactor();
    rthis.doStopActor(_rctx);
}

//-----------------------------------------------------------------------------

/*static*/ void Reactor::stop_actor_repost(ReactorContext& _rctx, EventBase&&)
{
    Reactor& rthis = _rctx.reactor();

    rthis.doStopActorRepost(_rctx, rthis.impl_->dummyCompletionHandlerUid());
}

//-----------------------------------------------------------------------------

/*NOTE:
    We do not stop the actor rightaway - we make sure that any
    pending Events are delivered to the actor before we stop
*/
void Reactor::postActorStop(ReactorContext& _rctx)
{
    doPostActorStop(_rctx, impl_->dummyCompletionHandlerUid());
}

//-----------------------------------------------------------------------------

void Reactor::doStopActor(ReactorContext& _rctx)
{
    ActorStub& ras = this->impl_->actor_dq_[_rctx.actor_index_];

    this->stopActor(*ras.actor_ptr_, ras.pservice_->manager());

    ras.clear();
    --actor_count_;
    rstatistic_.actorCount(actor_count_);
    this->impl_->freeuid_vec_.push_back(UniqueId(_rctx.actor_index_, ras.unique_));
}

//-----------------------------------------------------------------------------

void Reactor::doCompleteIo(NanoTime const& _rcrttime, const size_t _sz)
{
    ReactorContext ctx(*this, _rcrttime);

    solid_log(logger, Verbose, "selcnt = " << _sz);

#if defined(SOLID_USE_EPOLL)
    for (size_t i = 0; i < _sz; ++i) {
        epoll_event&           rev = impl_->event_vec_[i];
        CompletionHandlerStub& rch = impl_->completion_handler_dq_[rev.data.u64];

        ctx.reactor_event_             = systemEventsToReactorEvents(rev.events);
        ctx.completion_heandler_index_ = rev.data.u64;
#elif defined(SOLID_USE_KQUEUE)
    for (size_t i = 0; i < _sz; ++i) {
        struct kevent&         rev = impl_->event_vec_[i];
        CompletionHandlerStub& rch = impl_->completion_handler_dq_[void_to_index(rev.udata)];

        ctx.reactor_event_             = systemEventsToReactorEvents(rev.flags, rev.filter);
        ctx.completion_heandler_index_ = void_to_index(rev.udata);
#elif defined(SOLID_USE_WSAPOLL)
    const size_t vecsz = impl_->event_vec_.size();
    size_t       evcnt = _sz;
    for (size_t i = 0; i < vecsz; ++i) {
        if (evcnt == 0)
            break;

        WSAPOLLFD& rev = impl_->event_vec_[i];
        if (rev.revents == 0 || rev.revents & POLLNVAL)
            continue;
        --evcnt;
        CompletionHandlerStub& rch     = impl_->completion_handler_dq_[i];
        ctx.reactor_event_             = systemEventsToReactorEvents(rev.revents, rev.events);
        ctx.completion_heandler_index_ = i;
        if (rch.connect_idx_ != InvalidIndex()) {
            // we have events on a connecting socket
            // so we remove it from connect waiting list
            remConnect(ctx);
        }
#endif
        ctx.actor_index_ = rch.actor_idx_;

        rch.pcompletion_handler_->handleCompletion(ctx);
        ctx.clearError();
    }
#if defined(SOLID_USE_WSAPOLL)
    for (size_t j = 0; j < impl_->connect_vec_.size();) {
        const size_t           i   = impl_->connect_vec_[j];
        WSAPOLLFD&             rev = impl_->event_vec_[i];
        CompletionHandlerStub& rch = impl_->completion_handler_dq_[i];

        if (SocketDevice::error(rev.fd)) {

            ctx.reactor_event_             = systemEventsToReactorEvents(POLLERR, rev.events);
            ctx.completion_heandler_index_ = i;
            ctx.actor_index_               = rch.actor_idx_;

            remConnect(ctx);

            rch.pcompletion_handler_->handleCompletion(ctx);
            ctx.clearError();
        } else {
            ++j;
        }
    }
#endif
}

//-----------------------------------------------------------------------------
#if defined(SOLID_USE_WSAPOLL)
void Reactor::addConnect(ReactorContext& _rctx)
{
    CompletionHandlerStub& rch = impl_->completion_handler_dq_[_rctx.completion_heandler_index_];
    rch.connect_idx_           = impl_->connect_vec_.size();
    impl_->connect_vec_.emplace_back(_rctx.completion_heandler_index_);
}

void Reactor::remConnect(ReactorContext& _rctx)
{
    CompletionHandlerStub& rch                                             = impl_->completion_handler_dq_[_rctx.completion_heandler_index_];
    impl_->connect_vec_[rch.connect_idx_]                                  = impl_->connect_vec_.back();
    impl_->completion_handler_dq_[impl_->connect_vec_.back()].connect_idx_ = rch.connect_idx_;
    rch.connect_idx_                                                       = InvalidIndex();
    impl_->connect_vec_.pop_back();
}
#endif
//-----------------------------------------------------------------------------

void Reactor::onTimer(ReactorContext& _rctx, const size_t _chidx)
{
    CompletionHandlerStub& rch = impl_->completion_handler_dq_[_chidx];

    _rctx.reactor_event_             = ReactorEventE::Timer;
    _rctx.completion_heandler_index_ = _chidx;
    _rctx.actor_index_               = rch.actor_idx_;

    rch.pcompletion_handler_->handleCompletion(_rctx);
    _rctx.clearError();
}

//-----------------------------------------------------------------------------

void Reactor::doCompleteTimer(NanoTime const& _rcrttime)
{
    ReactorContext ctx(*this, _rcrttime);
    impl_->time_store_.pop(
        _rcrttime,
        [this, &ctx](const size_t _chidx, const NanoTime& /* _expiry */, const size_t /* _proxy_index */) {
            onTimer(ctx, _chidx);
        });
}

//-----------------------------------------------------------------------------
void Reactor::doCompleteEvents(ReactorContext const& _rctx)
{
    doCompleteEvents(_rctx.nanoTime(), impl_->dummyCompletionHandlerUid());
}

void Reactor::doCompleteEvents(NanoTime const& _rcrttime)
{
    ReactorContext ctx(*this, _rcrttime);
    doCompleteEvents(ctx);
}

//-----------------------------------------------------------------------------

/*static*/ void Reactor::call_actor_on_event(ReactorContext& _rctx, EventBase&& _uevent)
{
    _rctx.actor().onEvent(_rctx, std::move(_uevent));
}

//-----------------------------------------------------------------------------

/*static*/ void Reactor::increase_event_vector_size(ReactorContext& _rctx, EventBase&& /*_rev*/)
{
    Reactor& rthis = _rctx.reactor();

    solid_log(logger, Info, "" << rthis.impl_->device_count_ << " >= " << rthis.impl_->event_vec_.size());

    if (rthis.impl_->device_count_ >= rthis.impl_->event_vec_.size()) {
        rthis.impl_->event_vec_.resize(rthis.impl_->device_count_);
        rthis.impl_->event_vec_.resize(rthis.impl_->event_vec_.capacity());
    }
}

//-----------------------------------------------------------------------------

bool Reactor::addDevice(ReactorContext& _rctx, Device const& _rsd, const ReactorWaitRequestE _req)
{
    solid_log(logger, Info, _rsd.descriptor());

    // solid_assert(_rctx.channel_index_ == _rch.idxreactor);

#if defined(SOLID_USE_EPOLL)
    epoll_event ev;
    ev.data.u64 = _rctx.completion_heandler_index_;
    ev.events   = reactorRequestsToSystemEvents(_req);

    if (epoll_ctl(impl_->reactor_fd_, EPOLL_CTL_ADD, _rsd.Device::descriptor(), &ev) != 0) {
        solid_throw_log(logger, "epoll_ctl: " << last_system_error().message());
        return false;
    }
    ++impl_->device_count_;
    if (impl_->device_count_ == (impl_->event_vec_.size() + 1)) {
        impl_->event_actor_ptr_->post(_rctx, &Reactor::increase_event_vector_size);
    }
#elif defined(SOLID_USE_KQUEUE)
    int read_flags  = EV_ADD;
    int write_flags = EV_ADD;

    switch (_req) {
    case ReactorWaitRequestE::None:
        read_flags |= EV_DISABLE;
        write_flags |= EV_DISABLE;
    case ReactorWaitRequestE::Read:
        read_flags |= EV_ENABLE;
        write_flags |= EV_DISABLE;
        break;
    case ReactorWaitRequestE::Write:
        read_flags |= EV_DISABLE;
        write_flags |= EV_ENABLE;
        break;
    case ReactorWaitRequestE::ReadOrWrite:
        read_flags |= EV_ENABLE;
        write_flags |= EV_ENABLE;
        break;
    case ReactorWaitRequestE::User: {
        struct kevent ev;
        EV_SET(&ev, _rsd.descriptor(), EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, index_to_void(_rctx.completion_heandler_index_));
        if (kevent(impl_->reactor_fd_, &ev, 1, nullptr, 0, nullptr)) {
            solid_log(logger, Error, "kevent: " << last_system_error().message());
            solid_assert_log(false, logger);
            return false;
        } else {
            ++impl_->device_count_;
            if (impl_->device_count_ == (impl_->event_vec_.size() + 1)) {
                impl_->event_actor_ptr_->post(_rctx, &Reactor::increase_event_vector_size);
            }
        }
        return true;
    }
    default:
        solid_assert_log(false, logger);
        return false;
    }

    struct kevent ev[2];
    EV_SET(&ev[0], _rsd.descriptor(), EVFILT_READ, read_flags | EV_CLEAR, 0, 0, index_to_void(_rctx.completion_heandler_index_));
    EV_SET(&ev[1], _rsd.descriptor(), EVFILT_WRITE, write_flags | EV_CLEAR, 0, 0, index_to_void(_rctx.completion_heandler_index_));
    if (kevent(impl_->reactor_fd_, ev, 2, nullptr, 0, nullptr)) {
        solid_log(logger, Error, "kevent: " << last_system_error().message());
        solid_assert_log(false, logger);
        return false;
    } else {
        ++impl_->device_count_;
        if (impl_->device_count_ == (impl_->event_vec_.size() + 1)) {
            impl_->event_actor_ptr_->post(_rctx, &Reactor::increase_event_vector_size);
        }
    }
#elif defined(SOLID_USE_WSAPOLL)
    if (_rctx.completion_heandler_index_ >= impl_->event_vec_.size()) {
        impl_->event_vec_.resize(_rctx.completion_heandler_index_ + 1);
    }

    impl_->event_vec_[_rctx.completion_heandler_index_].fd     = reinterpret_cast<SocketDevice::DescriptorT>(_rsd.descriptor());
    impl_->event_vec_[_rctx.completion_heandler_index_].events = reactorRequestsToSystemEvents(_req);
#endif
    return true;
}

//-----------------------------------------------------------------------------

bool Reactor::modDevice(ReactorContext& _rctx, Device const& _rsd, const ReactorWaitRequestE _req)
{
    solid_log(logger, Info, _rsd.descriptor());
#if defined(SOLID_USE_EPOLL)
    epoll_event ev;

    ev.data.u64 = _rctx.completion_heandler_index_;
    ev.events   = reactorRequestsToSystemEvents(_req);

    if (epoll_ctl(impl_->reactor_fd_, EPOLL_CTL_MOD, _rsd.Device::descriptor(), &ev) != 0) {
        solid_throw_log(logger, "epoll_ctl: " << last_system_error().message());
        return false;
    }
#elif defined(SOLID_USE_KQUEUE)
    int           read_flags  = 0;
    int           write_flags = 0;
    struct kevent ev[2];

    switch (_req) {
    case ReactorWaitRequestE::None:
        read_flags |= EV_DISABLE;
        write_flags |= EV_DISABLE;
    case ReactorWaitRequestE::Read:
        read_flags |= (EV_ENABLE | EV_CLEAR);
        write_flags |= EV_DISABLE;
        break;
    case ReactorWaitRequestE::Write:
        read_flags |= EV_DISABLE;
        write_flags |= (EV_ENABLE | EV_CLEAR);
        break;
    case ReactorWaitRequestE::ReadOrWrite:
        read_flags |= (EV_ENABLE | EV_CLEAR);
        write_flags |= (EV_ENABLE | EV_CLEAR);
        break;
    case ReactorWaitRequestE::User: {

        struct kevent ev;

        EV_SET(&ev, _rsd.descriptor(), EVFILT_USER, EV_ADD, 0, 0, index_to_void(_rctx.completion_heandler_index_));

        if (kevent(impl_->reactor_fd_, &ev, 1, nullptr, 0, nullptr)) {
            solid_log(logger, Error, "kevent: " << last_system_error().message());
            solid_assert_log(false, logger);
            return false;
        } else {
            ++impl_->device_count_;
            if (impl_->device_count_ == (impl_->event_vec_.size() + 1)) {
                impl_->event_actor_ptr_->post(_rctx, &Reactor::increase_event_vector_size);
            }
        }
        return true;
    }
    default:
        solid_assert_log(false, logger);
        return false;
    }

    EV_SET(&ev[0], _rsd.descriptor(), EVFILT_READ, read_flags, 0, 0, index_to_void(_rctx.completion_heandler_index_));
    EV_SET(&ev[1], _rsd.descriptor(), EVFILT_WRITE, write_flags, 0, 0, index_to_void(_rctx.completion_heandler_index_));

    if (kevent(impl_->reactor_fd_, ev, 2, nullptr, 0, nullptr)) {
        solid_log(logger, Error, "kevent: " << last_system_error().message());
        solid_assert_log(false, logger);
        return false;
    } else {
        ++impl_->device_count_;
        if (impl_->device_count_ == (impl_->event_vec_.size() + 1)) {
            impl_->event_actor_ptr_->post(_rctx, &Reactor::increase_event_vector_size);
        }
    }
#elif defined(SOLID_USE_WSAPOLL)
    if (_req != ReactorWaitRequestE::None) {
        impl_->event_vec_[_rctx.completion_heandler_index_].events |= reactorRequestsToSystemEvents(_req);
    } else {
        impl_->event_vec_[_rctx.completion_heandler_index_].events = reactorRequestsToSystemEvents(_req);
    }
#endif
    return true;
}

//-----------------------------------------------------------------------------

bool Reactor::remDevice(CompletionHandler const& _rch, Device const& _rsd)
{
    solid_log(logger, Info, _rsd.descriptor());
#if defined(SOLID_USE_EPOLL)
    epoll_event ev;

    if (!_rsd) {
        return false;
    }

    if (epoll_ctl(impl_->reactor_fd_, EPOLL_CTL_DEL, _rsd.Device::descriptor(), &ev) != 0) {
        solid_throw_log(logger, "epoll_ctl: " << last_system_error().message());
        return false;
    }

    --impl_->device_count_;
#elif defined(SOLID_USE_KQUEUE)
    struct kevent ev[2];

    if (_rsd) {
        EV_SET(&ev[0], _rsd.descriptor(), EVFILT_READ, EV_DELETE, 0, 0, 0);
        EV_SET(&ev[1], _rsd.descriptor(), EVFILT_WRITE, EV_DELETE, 0, 0, 0);
        if (kevent(impl_->reactor_fd_, ev, 2, nullptr, 0, nullptr)) {
            solid_log(logger, Error, "kevent: " << last_system_error().message());
            solid_assert_log(false, logger);
            return false;
        } else {
            --impl_->device_count_;
        }
    } else {
        EV_SET(ev, _rsd.descriptor(), EVFILT_USER, EV_DELETE, 0, 0, 0);
        if (kevent(impl_->reactor_fd_, ev, 1, nullptr, 0, nullptr)) {
            solid_log(logger, Error, "kevent: " << last_system_error().message());
            solid_assert_log(false, logger);
            return false;
        } else {
            --impl_->device_count_;
        }
    }
#elif defined(SOLID_USE_WSAPOLL)
    if (!_rsd) {
        return false;
    }
    impl_->event_vec_[_rch.idxreactor].clear();
#endif
    return true;
}

//-----------------------------------------------------------------------------

bool Reactor::addTimer(CompletionHandler const& _rch, NanoTime const& _rt, size_t& _rstoreidx)
{
    if (_rstoreidx != InvalidIndex()) {
        impl_->time_store_.update(_rstoreidx, impl_->current_time_, _rt);
    } else {
        _rstoreidx = impl_->time_store_.push(impl_->current_time_, _rt, _rch.idxreactor);
    }
    return true;
}

//-----------------------------------------------------------------------------

void Reactor::doUpdateTimerIndex(const size_t _chidx, const size_t _newidx, const size_t _oldidx)
{
    CompletionHandlerStub& rch = impl_->completion_handler_dq_[_chidx];
    solid_assert_log(static_cast<SteadyTimer*>(rch.pcompletion_handler_)->storeidx_ == _oldidx, logger);
    static_cast<SteadyTimer*>(rch.pcompletion_handler_)->storeidx_ = _newidx;
}

//-----------------------------------------------------------------------------

bool Reactor::remTimer(CompletionHandler const& /*_rch*/, size_t const& _rstoreidx)
{
    if (_rstoreidx != InvalidIndex()) {
        impl_->time_store_.pop(_rstoreidx);
    }
    return true;
}

//-----------------------------------------------------------------------------

void Reactor::registerCompletionHandler(CompletionHandler& _rch, Actor const& _ract)
{
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

    _rch.idxreactor = idx;

    solid_log(logger, Info, "idx " << idx << " chdq.size = " << impl_->completion_handler_dq_.size() << " this " << this);

    {
        NanoTime       dummytime;
        ReactorContext ctx(*this, dummytime);

        ctx.reactor_event_             = ReactorEventE::Init;
        ctx.actor_index_               = rcs.actor_idx_;
        ctx.completion_heandler_index_ = idx;

        _rch.handleCompletion(ctx);
    }
}

//-----------------------------------------------------------------------------

void Reactor::unregisterCompletionHandler(CompletionHandler& _rch)
{
    solid_log(logger, Info, "");

    CompletionHandlerStub& rcs = impl_->completion_handler_dq_[_rch.idxreactor];

    {
        NanoTime       dummytime;
        ReactorContext ctx(*this, dummytime);

        ctx.reactor_event_             = ReactorEventE::Clear;
        ctx.actor_index_               = rcs.actor_idx_;
        ctx.completion_heandler_index_ = _rch.idxreactor;

        _rch.handleCompletion(ctx);
    }

    impl_->completion_handler_index_stk_.push(_rch.idxreactor);
    rcs.pcompletion_handler_ = &impl_->event_actor_ptr_->dummy_handler_;
    rcs.actor_idx_           = 0;
    ++rcs.unique_;
}

//-----------------------------------------------------------------------------

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
    solid_log(logger, Verbose, "");
    return *safeSpecific();
}
} // namespace impl
//-----------------------------------------------------------------------------
//      ReactorContext
//-----------------------------------------------------------------------------

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

Service::ActorMutexT& ReactorContext::actorMutex() const
{
    return service().mutex(actor());
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

CompletionHandler* ReactorContext::completionHandler() const
{
    return reactor().completionHandler(*this);
}

//-----------------------------------------------------------------------------
// EventHandler
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
void EventHandler::write(impl::Reactor& _rreactor)
{
#if defined(SOLID_USE_EPOLL)
    const uint64_t v = 1;
    dev_.write(reinterpret_cast<const char*>(&v), sizeof(v));
#elif defined(SOLID_USE_KQUEUE)
    struct kevent ev;

    solid_log(logger, Verbose, "trigger user event");

    EV_SET(&ev, dev_.descriptor(), EVFILT_USER, 0, NOTE_TRIGGER, 0, index_to_void(this->indexWithinReactor()));

    if (kevent(_rreactor.impl_->reactor_fd_, &ev, 1, nullptr, 0, nullptr)) {
        solid_log(logger, Error, "kevent: " << last_system_error().message());
        solid_assert_log(false, logger);
    }
#elif defined(SOLID_USE_WSAPOLL)
    const uint32_t v = 1;
    bool           can_retry;
    ErrorCodeT     err;
    dev_.send(reinterpret_cast<const char*>(&v), sizeof(v), can_retry, err);
#endif
}

/*static*/ void EventHandler::on_init(CompletionHandler& _rch, ReactorContext& _rctx)
{
    EventHandler& rthis = static_cast<EventHandler&>(_rch);

    rthis.contextBind(_rctx);

#if defined(SOLID_USE_EPOLL)
    rthis.reactor(_rctx).addDevice(_rctx, rthis.dev_, ReactorWaitRequestE::Read);
#elif defined(SOLID_USE_KQUEUE)
    rthis.reactor(_rctx).addDevice(_rctx, rthis.dev_, ReactorWaitRequestE::User);
#elif defined(SOLID_USE_WSAPOLL)
    rthis.reactor(_rctx).addDevice(_rctx, rthis.dev_, ReactorWaitRequestE::Read);
#endif
    rthis.completionCallback(&on_completion);
}

//-----------------------------------------------------------------------------

/*static*/ void EventHandler::on_completion(CompletionHandler& _rch, ReactorContext& _rctx)
{
    EventHandler& rthis = static_cast<EventHandler&>(_rch);
#if defined(SOLID_USE_EPOLL)
    uint64_t v = -1;
    ssize_t  rv;

    do {
        rv = rthis.dev_.read(reinterpret_cast<char*>(&v), sizeof(v));
        solid_log(logger, Info, "Read from event " << rv << " value = " << v);
    } while (rv == sizeof(v));
#elif defined(SOLID_USE_WSAPOLL)
    constexpr size_t buf_sz = 64;
    char             buf[buf_sz];
    bool             can_retry;
    ErrorCodeT       err;
    rthis.dev_.recv(buf, buf_sz, can_retry, err);
    rthis.contextBind(_rctx);
    rthis.reactor(_rctx).modDevice(_rctx, rthis.dev_, ReactorWaitRequestE::Read);
#endif
    rthis.reactor(_rctx).doCompleteEvents(_rctx);
}

//-----------------------------------------------------------------------------

bool EventHandler::init()
{
#if defined(SOLID_USE_EPOLL)
    dev_ = Device(eventfd(0, EFD_NONBLOCK));

    if (!dev_) {
        solid_log(logger, Error, "eventfd: " << last_system_error().message());
        return false;
    }
#elif defined(SOLID_USE_WSAPOLL)
    ErrorCodeT err;
    err = dev_.create(SocketInfo::Inet4, SocketInfo::Datagram);
    if (err)
        return false;

    err = dev_.bind(SocketAddressInet4("127.0.0.1", 0));
    if (err)
        return false;

    SocketAddress sa;
    err = dev_.localAddress(sa);
    if (err)
        return false;
    dev_.enableLoopbackFastPath();
    err = dev_.connect(SocketAddressInet4("127.0.0.1", sa.port()));
    if (err)
        return false;

    dev_.makeNonBlocking();
#endif
    return true;
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

} // namespace aio
} // namespace frame
} // namespace solid
