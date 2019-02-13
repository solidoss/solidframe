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
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <queue>
#include <vector>

#include "solid/system/common.hpp"
#include "solid/system/device.hpp"
#include "solid/system/error.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/nanotime.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

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

} //namespace

typedef std::atomic<bool>   AtomicBoolT;
typedef std::atomic<size_t> AtomicSizeT;
typedef Reactor::TaskT      TaskT;

class EventActor : public Actor {
public:
    EventActor()
        : dummyhandler(proxy(), dummy_completion)
    {
        use();
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
        UniqueId const& _ruid, TaskT const& _ractptr, Service& _rsvc, Event const& _revent)
        : uid(_ruid)
        , actptr(_ractptr)
        , rsvc(_rsvc)
        , event(_revent)
    {
    }

    //NewTaskStub(){}
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
typedef TimeStore<size_t>                 TimeStoreT;

struct Reactor::Data {
    Data(

        )
        : running(false)
        , must_stop(false)
        , crtpushtskvecidx(0)
        , crtraisevecidx(0)
        , crtpushvecsz(0)
        , crtraisevecsz(0)
        , objcnt(0)
        , timestore(MinEventCapacity)
    {
        pcrtpushtskvec = &pushtskvec[1];
        pcrtraisevec   = &raisevec[1];
    }

    int computeWaitTimeMilliseconds(NanoTime const& _rcrt) const
    {
        if (!exeq.empty()) {
            return 0;
        }

        if (timestore.size() != 0u) {
            if (_rcrt < timestore.next()) {
                const int64_t maxwait = 1000 * 60; //1 minute
                int64_t       diff    = 0;
                //                 NanoTime    delta = timestore.next();
                //                 delta -= _rcrt;
                //                 diff = (delta.seconds() * 1000);
                //                 diff += (delta.nanoSeconds() / 1000000);
                const auto crt_tp  = _rcrt.timePointCast<std::chrono::steady_clock::time_point>();
                const auto next_tp = timestore.next().timePointCast<std::chrono::steady_clock::time_point>();
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
        const size_t idx = eventact.dummyhandler.idxreactor;
        return UniqueId(idx, chdq[idx].unique);
    }

    bool                    running;
    bool                    must_stop;
    size_t                  crtpushtskvecidx;
    size_t                  crtraisevecidx;
    size_t                  crtpushvecsz;
    size_t                  crtraisevecsz;
    size_t                  objcnt;
    TimeStoreT              timestore;
    NewTaskVectorT*         pcrtpushtskvec;
    RaiseEventVectorT*      pcrtraisevec;
    mutex                   mtx;
    condition_variable      cnd;
    NewTaskVectorT          pushtskvec[2];
    RaiseEventVectorT       raisevec[2];
    EventActor              eventact;
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
    solid_dbg(logger, Verbose, "");
}

Reactor::~Reactor()
{
    solid_dbg(logger, Verbose, "");
}

bool Reactor::start()
{
    doStoreSpecific();
    solid_dbg(logger, Verbose, "");

    impl_->actdq.push_back(ActorStub());
    impl_->actdq.back().actptr = &impl_->eventact;

    popUid(*impl_->actdq.back().actptr);

    impl_->eventact.registerCompletionHandlers();

    impl_->running = true;

    return true;
}

/*virtual*/ bool Reactor::raise(UniqueId const& _ractuid, Event const& _revent)
{
    solid_dbg(logger, Verbose, (void*)this << " uid = " << _ractuid.index << ',' << _ractuid.unique << " event = " << _revent);
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
    solid_dbg(logger, Verbose, (void*)this << " uid = " << _ractuid.index << ',' << _ractuid.unique << " event = " << _uevent);
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
    solid_dbg(logger, Verbose, "");
    lock_guard<mutex> lock(impl_->mtx);
    impl_->must_stop = true;
    impl_->cnd.notify_one();
}

//Called from outside reactor's thread
bool Reactor::push(TaskT& _robj, Service& _rsvc, Event const& _revent)
{
    solid_dbg(logger, Verbose, (void*)this);
    bool   rv        = true;
    size_t pushvecsz = 0;
    {
        lock_guard<mutex> lock(impl_->mtx);
        const UniqueId    uid = this->popUid(*_robj);

        solid_dbg(logger, Verbose, (void*)this << " uid = " << uid.index << ',' << uid.unique << " event = " << _revent);

        impl_->pushtskvec[impl_->crtpushtskvecidx].push_back(NewTaskStub(uid, _robj, _rsvc, _revent));
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
    solid_dbg(logger, Verbose, "<enter>");
    bool     running = true;
    NanoTime crttime;

    while (running) {

        crtload = impl_->objcnt + impl_->exeq.size();

        if (doWaitEvent(crttime)) {
            crttime = std::chrono::steady_clock::now();
            doCompleteEvents(crttime);
        }
        crttime = std::chrono::steady_clock::now();
        doCompleteTimer(crttime);

        crttime = std::chrono::steady_clock::now();
        doCompleteExec(crttime);

        running = impl_->running || (impl_->objcnt != 0) || !impl_->exeq.empty();
    }
    impl_->eventact.stop();
    doClearSpecific();
    solid_dbg(logger, Verbose, "<exit>");
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

void Reactor::doPost(ReactorContext& _rctx, EventFunctionT& _revfn, Event&& _uev)
{
    impl_->exeq.push(ExecStub(_rctx.actorUid(), std::move(_uev)));
    solid_function_clear(impl_->exeq.back().exefnc);
    std::swap(impl_->exeq.back().exefnc, _revfn);
    impl_->exeq.back().chnuid = impl_->dummyCompletionHandlerUid();
}

void Reactor::doPost(ReactorContext& _rctx, EventFunctionT& _revfn, Event&& _uev, CompletionHandler const& _rch)
{
    impl_->exeq.push(ExecStub(_rctx.actorUid(), std::move(_uev)));
    solid_function_clear(impl_->exeq.back().exefnc);
    std::swap(impl_->exeq.back().exefnc, _revfn);
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
    We do not stop the object rightaway - we make sure that any
    pending Events are delivered to the object before we stop
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

    ros.actptr.clear();
    ros.psvc = nullptr;
    ++ros.unique;
    --this->impl_->objcnt;
    this->impl_->freeuidvec.push_back(UniqueId(_rctx.actidx, ros.unique));
}

struct ChangeTimerIndexCallback {
    Reactor& r;
    ChangeTimerIndexCallback(Reactor& _r)
        : r(_r)
    {
    }

    void operator()(const size_t _chidx, const size_t _newidx, const size_t _oldidx) const
    {
        r.doUpdateTimerIndex(_chidx, _newidx, _oldidx);
    }
};

struct TimerCallback {
    Reactor&        r;
    ReactorContext& rctx;
    TimerCallback(Reactor& _r, ReactorContext& _rctx)
        : r(_r)
        , rctx(_rctx)
    {
    }

    void operator()(const size_t _tidx, const size_t _chidx) const
    {
        r.onTimer(rctx, _tidx, _chidx);
    }
};

void Reactor::onTimer(ReactorContext& _rctx, const size_t /*_tidx*/, const size_t _chidx)
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
    TimerCallback  tcbk(*this, ctx);
    impl_->timestore.pop(_rcrttime, tcbk, ChangeTimerIndexCallback(*this));
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
    solid_dbg(logger, Verbose, "");

    NewTaskVectorT&    crtpushvec  = *impl_->pcrtpushtskvec;
    RaiseEventVectorT& crtraisevec = *impl_->pcrtraisevec;
    ReactorContext     ctx(*this, _rcrttime);

    if (!crtpushvec.empty()) {

        impl_->objcnt += crtpushvec.size();

        for (auto& rnewact : crtpushvec) {

            if (rnewact.uid.index >= impl_->actdq.size()) {
                impl_->actdq.resize(static_cast<size_t>(rnewact.uid.index + 1));
            }

            ActorStub& ros = impl_->actdq[static_cast<size_t>(rnewact.uid.index)];
            solid_assert(ros.unique == rnewact.uid.unique);

            {
                //NOTE: we must lock the mutex of the object
                //in order to ensure that object is fully registered onto the manager

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
        size_t idx = impl_->timestore.change(_rstoreidx, _rt);
        solid_assert(idx == _rch.idxreactor);
    } else {
        _rstoreidx = impl_->timestore.push(_rt, _rch.idxreactor);
    }
    return true;
}

void Reactor::doUpdateTimerIndex(const size_t _chidx, const size_t _newidx, const size_t _oldidx)
{
    CompletionHandlerStub& rch = impl_->chdq[_chidx];
    solid_assert(static_cast<SteadyTimer*>(rch.pch)->storeidx == _oldidx);
    static_cast<SteadyTimer*>(rch.pch)->storeidx = _newidx;
}

bool Reactor::remTimer(CompletionHandler const& /*_rch*/, size_t const& _rstoreidx)
{
    if (_rstoreidx != InvalidIndex()) {
        impl_->timestore.pop(_rstoreidx, ChangeTimerIndexCallback(*this));
    }
    return true;
}

void Reactor::registerCompletionHandler(CompletionHandler& _rch, Actor const& _ract)
{
    solid_dbg(logger, Verbose, "");
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
    solid_dbg(logger, Verbose, "");
    CompletionHandlerStub& rcs = impl_->chdq[_rch.idxreactor];
    {
        NanoTime       dummytime;
        ReactorContext ctx(*this, dummytime);

        ctx.reactevn = ReactorEventClear;
        ctx.actidx   = rcs.actidx;
        ctx.chnidx   = _rch.idxreactor;

        _rch.handleCompletion(ctx);
    }

    rcs.pch    = &impl_->eventact.dummyhandler;
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
    solid_dbg(logger, Verbose, "");
    return *safeSpecific();
}

//=============================================================================
//      ReactorContext
//=============================================================================

Actor& ReactorContext::actor() const
{
    return reactor().actor(*this);
}
Service& ReactorContext::service() const
{
    return reactor().service(*this);
}

//-----------------------------------------------------------------------------

Manager& ReactorContext::manager() const
{
    return reactor().service(*this).manager();
}

//-----------------------------------------------------------------------------

UniqueId ReactorContext::actorUid() const
{
    return reactor().actorUid(*this);
}

//-----------------------------------------------------------------------------

std::mutex& ReactorContext::objectMutex() const
{
    return reactor().service(*this).mutex(reactor().actor(*this));
}

//-----------------------------------------------------------------------------

CompletionHandler* ReactorContext::completionHandler() const
{
    return reactor().completionHandler(*this);
}

//=============================================================================
//      ReactorBase
//=============================================================================
UniqueId ReactorBase::popUid(ActorBase& _robj)
{
    UniqueId rv(crtidx, 0);
    if (!uidstk.empty()) {
        rv = uidstk.top();
        uidstk.pop();
    } else {
        ++crtidx;
    }
    _robj.runId(rv);
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

} //namespace frame
} //namespace solid
