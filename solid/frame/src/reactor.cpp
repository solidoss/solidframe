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
#include "solid/system/debug.hpp"
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

#include "solid/frame/common.hpp"
#include "solid/frame/completion.hpp"
#include "solid/frame/object.hpp"
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

class EventObject : public Object {
public:
    EventObject()
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
        Object::post(_rctx, _f);
    }

    CompletionHandler dummyhandler;
};

struct NewTaskStub {
    NewTaskStub(
        UniqueId const& _ruid, TaskT const& _robjptr, Service& _rsvc, Event const& _revent)
        : uid(_ruid)
        , objptr(_robjptr)
        , rsvc(_rsvc)
        , event(_revent)
    {
    }

    //NewTaskStub(){}
    UniqueId uid;
    TaskT    objptr;
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
        RaiseEventStub&& _ures)
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
        const size_t       _objidx = InvalidIndex())
        : pch(_pch)
        , objidx(_objidx)
        , unique(0)
    {
    }

    CompletionHandler* pch;
    size_t             objidx;
    UniqueT            unique;
};

struct ObjectStub {
    ObjectStub()
        : unique(0)
        , psvc(nullptr)
    {
    }

    UniqueT  unique;
    Service* psvc;
    TaskT    objptr;
};

enum {
    MinEventCapacity = 32,
    MaxEventCapacity = 1024 * 64
};

struct ExecStub {
    template <class F>
    ExecStub(
        UniqueId const& _ruid, F _f, Event&& _uevent = Event())
        : objuid(_ruid)
        , exefnc(_f)
        , event(std::move(_uevent))
    {
    }

    template <class F>
    ExecStub(
        UniqueId const& _ruid, F _f, UniqueId const& _rchnuid, Event&& _revent = Event())
        : objuid(_ruid)
        , chnuid(_rchnuid)
        , exefnc(_f)
        , event(std::move(_revent))
    {
    }

    ExecStub(
        UniqueId const& _ruid, Event const& _revent = Event())
        : objuid(_ruid)
        , event(_revent)
    {
    }

    ExecStub(
        ExecStub&& _res)
        : objuid(std::move(_res.objuid))
        , chnuid(std::move(_res.chnuid))
        , event(std::move(_res.event))
    {
        std::swap(exefnc, _res.exefnc);
    }

    UniqueId                objuid;
    UniqueId                chnuid;
    Reactor::EventFunctionT exefnc;
    Event                   event;
};

typedef std::vector<NewTaskStub>          NewTaskVectorT;
typedef std::vector<RaiseEventStub>       RaiseEventVectorT;
typedef std::deque<CompletionHandlerStub> CompletionHandlerDequeT;
typedef std::vector<UniqueId>             UidVectorT;
typedef std::deque<ObjectStub>            ObjectDequeT;
typedef Queue<ExecStub>                   ExecQueueT;
typedef Stack<size_t>                     SizeStackT;
typedef TimeStore<size_t>                 TimeStoreT;

struct Reactor::Data {
    Data(

        )
        : running(0)
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
        if (exeq.size()) {
            return 0;
        } else if (timestore.size()) {
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
                } else {
                    return diff;
                }
            } else {
                return 0;
            }
        } else {
            return -1;
        }
    }

    UniqueId dummyCompletionHandlerUid() const
    {
        const size_t idx = eventobj.dummyhandler.idxreactor;
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
    EventObject             eventobj;
    CompletionHandlerDequeT chdq;
    UidVectorT              freeuidvec;
    ObjectDequeT            objdq;
    ExecQueueT              exeq;
    SizeStackT              chposcache;
};

Reactor::Reactor(
    SchedulerBase& _rsched,
    const size_t   _idx)
    : ReactorBase(_rsched, _idx)
    , impl_(make_pimpl<Data>())
{
    vdbgx(Debug::aio, "");
}

Reactor::~Reactor()
{
    vdbgx(Debug::aio, "");
}

bool Reactor::start()
{
    doStoreSpecific();
    vdbgx(Debug::aio, "");

    impl_->objdq.push_back(ObjectStub());
    impl_->objdq.back().objptr = &impl_->eventobj;

    popUid(*impl_->objdq.back().objptr);

    impl_->eventobj.registerCompletionHandlers();

    impl_->running = true;

    return true;
}

/*virtual*/ bool Reactor::raise(UniqueId const& _robjuid, Event const& _revent)
{
    vdbgx(Debug::aio, (void*)this << " uid = " << _robjuid.index << ',' << _robjuid.unique << " event = " << _revent);
    bool   rv         = true;
    size_t raisevecsz = 0;
    {
        unique_lock<mutex> lock(impl_->mtx);

        impl_->raisevec[impl_->crtraisevecidx].push_back(RaiseEventStub(_robjuid, _revent));
        raisevecsz           = impl_->raisevec[impl_->crtraisevecidx].size();
        impl_->crtraisevecsz = raisevecsz;
        if (raisevecsz == 1) {
            impl_->cnd.notify_one();
        }
    }
    return rv;
}

/*virtual*/ bool Reactor::raise(UniqueId const& _robjuid, Event&& _uevent)
{
    vdbgx(Debug::aio, (void*)this << " uid = " << _robjuid.index << ',' << _robjuid.unique << " event = " << _uevent);
    bool   rv         = true;
    size_t raisevecsz = 0;
    {
        unique_lock<mutex> lock(impl_->mtx);

        impl_->raisevec[impl_->crtraisevecidx].push_back(RaiseEventStub(_robjuid, std::move(_uevent)));
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
    vdbgx(Debug::aio, "");
    unique_lock<mutex> lock(impl_->mtx);
    impl_->must_stop = true;
    impl_->cnd.notify_one();
}

//Called from outside reactor's thread
bool Reactor::push(TaskT& _robj, Service& _rsvc, Event const& _revent)
{
    vdbgx(Debug::aio, (void*)this);
    bool   rv        = true;
    size_t pushvecsz = 0;
    {
        unique_lock<mutex> lock(impl_->mtx);
        const UniqueId     uid = this->popUid(*_robj);

        vdbgx(Debug::aio, (void*)this << " uid = " << uid.index << ',' << uid.unique << " event = " << _revent);

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
    vdbgx(Debug::aio, "<enter>");
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
    impl_->eventobj.stop();
    doClearSpecific();
    vdbgx(Debug::aio, "<exit>");
}

UniqueId Reactor::objectUid(ReactorContext const& _rctx) const
{
    return UniqueId(_rctx.objidx, impl_->objdq[_rctx.objidx].unique);
}

Service& Reactor::service(ReactorContext const& _rctx) const
{
    return *impl_->objdq[_rctx.objidx].psvc;
}

Object& Reactor::object(ReactorContext const& _rctx) const
{
    return *impl_->objdq[_rctx.objidx].objptr;
}

CompletionHandler* Reactor::completionHandler(ReactorContext const& _rctx) const
{
    return impl_->chdq[_rctx.chnidx].pch;
}

void Reactor::doPost(ReactorContext& _rctx, EventFunctionT& _revfn, Event&& _uev)
{
    impl_->exeq.push(ExecStub(_rctx.objectUid(), std::move(_uev)));
    SOLID_FUNCTION_CLEAR(impl_->exeq.back().exefnc);
    std::swap(impl_->exeq.back().exefnc, _revfn);
    impl_->exeq.back().chnuid = impl_->dummyCompletionHandlerUid();
}

void Reactor::doPost(ReactorContext& _rctx, EventFunctionT& _revfn, Event&& _uev, CompletionHandler const& _rch)
{
    impl_->exeq.push(ExecStub(_rctx.objectUid(), std::move(_uev)));
    SOLID_FUNCTION_CLEAR(impl_->exeq.back().exefnc);
    std::swap(impl_->exeq.back().exefnc, _revfn);
    impl_->exeq.back().chnuid = UniqueId(_rch.idxreactor, impl_->chdq[_rch.idxreactor].unique);
}

/*static*/ void Reactor::stop_object_repost(ReactorContext& _rctx, Event&& _uevent)
{
    Reactor& rthis = _rctx.reactor();
    rthis.impl_->exeq.push(ExecStub(_rctx.objectUid()));
    rthis.impl_->exeq.back().exefnc = &stop_object;
    rthis.impl_->exeq.back().chnuid = rthis.impl_->dummyCompletionHandlerUid();
}

/*static*/ void Reactor::stop_object(ReactorContext& _rctx, Event&&)
{
    Reactor& rthis = _rctx.reactor();
    rthis.doStopObject(_rctx);
}

/*NOTE:
    We do not stop the object rightaway - we make sure that any
    pending Events are delivered to the object before we stop
*/
void Reactor::postObjectStop(ReactorContext& _rctx)
{
    impl_->exeq.push(ExecStub(_rctx.objectUid()));
    impl_->exeq.back().exefnc = &stop_object_repost;
    impl_->exeq.back().chnuid = impl_->dummyCompletionHandlerUid();
}

void Reactor::doStopObject(ReactorContext& _rctx)
{
    ObjectStub& ros = this->impl_->objdq[_rctx.objidx];

    this->stopObject(*ros.objptr, ros.psvc->manager());

    ros.objptr.clear();
    ros.psvc = nullptr;
    ++ros.unique;
    --this->impl_->objcnt;
    this->impl_->freeuidvec.push_back(UniqueId(_rctx.objidx, ros.unique));
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

void Reactor::onTimer(ReactorContext& _rctx, const size_t _tidx, const size_t _chidx)
{
    CompletionHandlerStub& rch = impl_->chdq[_chidx];

    _rctx.reactevn = ReactorEventTimer;
    _rctx.chnidx   = _chidx;
    _rctx.objidx   = rch.objidx;

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
    while (sz--) {
        ExecStub&              rexe(impl_->exeq.front());
        ObjectStub&            ros(impl_->objdq[rexe.objuid.index]);
        CompletionHandlerStub& rcs(impl_->chdq[rexe.chnuid.index]);

        if (ros.unique == rexe.objuid.unique && rcs.unique == rexe.chnuid.unique) {
            ctx.clearError();
            ctx.chnidx = rexe.chnuid.index;
            ctx.objidx = rexe.objuid.index;
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
    if (impl_->crtpushvecsz) {
        impl_->crtpushvecsz = 0;
        for (auto it = impl_->freeuidvec.begin(); it != impl_->freeuidvec.end(); ++it) {
            this->pushUid(*it);
        }
        impl_->freeuidvec.clear();

        const size_t crtpushvecidx = impl_->crtpushtskvecidx;
        impl_->crtpushtskvecidx    = ((crtpushvecidx + 1) & 1);
        impl_->pcrtpushtskvec      = &impl_->pushtskvec[crtpushvecidx];
        rv                         = true;
    }
    if (impl_->crtraisevecsz) {
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
    vdbgx(Debug::aio, "");

    NewTaskVectorT&    crtpushvec  = *impl_->pcrtpushtskvec;
    RaiseEventVectorT& crtraisevec = *impl_->pcrtraisevec;
    ReactorContext     ctx(*this, _rcrttime);

    if (crtpushvec.size()) {

        impl_->objcnt += crtpushvec.size();

        for (auto it = crtpushvec.begin(); it != crtpushvec.end(); ++it) {
            NewTaskStub& rnewobj(*it);

            if (rnewobj.uid.index >= impl_->objdq.size()) {
                impl_->objdq.resize(rnewobj.uid.index + 1);
            }

            ObjectStub& ros = impl_->objdq[rnewobj.uid.index];
            SOLID_ASSERT(ros.unique == rnewobj.uid.unique);

            {
                //NOTE: we must lock the mutex of the object
                //in order to ensure that object is fully registered onto the manager

                unique_lock<std::mutex> lock(rnewobj.rsvc.mutex(*rnewobj.objptr));
            }

            ros.objptr = std::move(rnewobj.objptr);
            ros.psvc   = &rnewobj.rsvc;

            ctx.clearError();
            ctx.chnidx = InvalidIndex();
            ctx.objidx = rnewobj.uid.index;

            ros.objptr->registerCompletionHandlers();

            impl_->exeq.push(ExecStub(rnewobj.uid, &call_object_on_event, impl_->dummyCompletionHandlerUid(), std::move(rnewobj.event)));
        }
        crtpushvec.clear();
    }

    if (crtraisevec.size()) {
        for (auto it = crtraisevec.begin(); it != crtraisevec.end(); ++it) {
            RaiseEventStub& revent = *it;
            impl_->exeq.push(ExecStub(revent.uid, &call_object_on_event, impl_->dummyCompletionHandlerUid(), std::move(revent.event)));
        }
        crtraisevec.clear();
    }
}

/*static*/ void Reactor::call_object_on_event(ReactorContext& _rctx, Event&& _uevent)
{
    _rctx.object().onEvent(_rctx, std::move(_uevent));
}

bool Reactor::addTimer(CompletionHandler const& _rch, NanoTime const& _rt, size_t& _rstoreidx)
{
    if (_rstoreidx != InvalidIndex()) {
        size_t idx = impl_->timestore.change(_rstoreidx, _rt);
        SOLID_ASSERT(idx == _rch.idxreactor);
    } else {
        _rstoreidx = impl_->timestore.push(_rt, _rch.idxreactor);
    }
    return true;
}

void Reactor::doUpdateTimerIndex(const size_t _chidx, const size_t _newidx, const size_t _oldidx)
{
    CompletionHandlerStub& rch = impl_->chdq[_chidx];
    SOLID_ASSERT(static_cast<SteadyTimer*>(rch.pch)->storeidx == _oldidx);
    static_cast<SteadyTimer*>(rch.pch)->storeidx = _newidx;
}

bool Reactor::remTimer(CompletionHandler const& _rch, size_t const& _rstoreidx)
{
    if (_rstoreidx != InvalidIndex()) {
        impl_->timestore.pop(_rstoreidx, ChangeTimerIndexCallback(*this));
    }
    return true;
}

void Reactor::registerCompletionHandler(CompletionHandler& _rch, Object const& _robj)
{
    vdbgx(Debug::aio, "");
    size_t idx;
    if (impl_->chposcache.size()) {
        idx = impl_->chposcache.top();
        impl_->chposcache.pop();
    } else {
        idx = impl_->chdq.size();
        impl_->chdq.push_back(CompletionHandlerStub());
    }
    CompletionHandlerStub& rcs = impl_->chdq[idx];
    rcs.objidx                 = _robj.ObjectBase::runId().index;
    rcs.pch                    = &_rch;
    //rcs.waitreq = ReactorWaitNone;
    _rch.idxreactor = idx;
    {
        NanoTime       dummytime;
        ReactorContext ctx(*this, dummytime);

        ctx.reactevn = ReactorEventInit;
        ctx.objidx   = rcs.objidx;
        ctx.chnidx   = idx;

        _rch.handleCompletion(ctx);
    }
}

void Reactor::unregisterCompletionHandler(CompletionHandler& _rch)
{
    vdbgx(Debug::aio, "");
    CompletionHandlerStub& rcs = impl_->chdq[_rch.idxreactor];
    {
        NanoTime       dummytime;
        ReactorContext ctx(*this, dummytime);

        ctx.reactevn = ReactorEventClear;
        ctx.objidx   = rcs.objidx;
        ctx.chnidx   = _rch.idxreactor;

        _rch.handleCompletion(ctx);
    }

    rcs.pch    = &impl_->eventobj.dummyhandler;
    rcs.objidx = 0;
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
    vdbgx(Debug::aio, "");
    return *safeSpecific();
}

//=============================================================================
//      ReactorContext
//=============================================================================

Object& ReactorContext::object() const
{
    return reactor().object(*this);
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

UniqueId ReactorContext::objectUid() const
{
    return reactor().objectUid(*this);
}

//-----------------------------------------------------------------------------

std::mutex& ReactorContext::objectMutex() const
{
    return reactor().service(*this).mutex(reactor().object(*this));
}

//-----------------------------------------------------------------------------

CompletionHandler* ReactorContext::completionHandler() const
{
    return reactor().completionHandler(*this);
}

//=============================================================================
//      ReactorBase
//=============================================================================
UniqueId ReactorBase::popUid(ObjectBase& _robj)
{
    UniqueId rv(crtidx, 0);
    if (uidstk.size()) {
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

} //namespace frame
} //namespace solid
