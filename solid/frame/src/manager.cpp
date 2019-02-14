// solid/frame/src/manager.cpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <deque>
#include <vector>

#include <climits>

#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/memory.hpp"
#include <atomic>

#include "solid/frame/actorbase.hpp"
#include "solid/frame/manager.hpp"
#include "solid/frame/reactorbase.hpp"
#include "solid/frame/service.hpp"

#include "solid/utility/event.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"

namespace solid {
namespace frame {

const LoggerT logger("solid::frame");

namespace {

enum {
    ErrorServiceUnknownE = 1,
    ErrorServiceNotRunningE,
    ErrorActorScheduleE
};

class ErrorCategory : public ErrorCategoryT {
public:
    ErrorCategory() {}

    const char* name() const noexcept override
    {
        return "solid::frame";
    }

    std::string message(int _ev) const override;
};

const ErrorCategory category;

std::string ErrorCategory::message(int _ev) const
{
    std::ostringstream oss;

    oss << "(" << name() << ":" << _ev << "): ";

    switch (_ev) {
    case 0:
        oss << "Success";
        break;
    case ErrorServiceUnknownE:
        oss << "Service not known";
        break;
    case ErrorServiceNotRunningE:
        oss << "Service not running";
        break;
    case ErrorActorScheduleE:
        oss << "Actor scheduling";
        break;
    default:
        oss << "Unknown";
        break;
    }
    return oss.str();
}
const ErrorConditionT error_service_unknown(ErrorServiceUnknownE, category);
const ErrorConditionT error_service_not_running(ErrorServiceNotRunningE, category);
const ErrorConditionT error_actor_schedule(ErrorActorScheduleE, category);

} //namespace

std::ostream& operator<<(std::ostream& _ros, UniqueId const& _uid)
{
    _ros << _uid.index << ':' << _uid.unique;
    return _ros;
}

typedef std::atomic<size_t>       AtomicSizeT;
typedef std::atomic<bool>         AtomicBoolT;
typedef std::atomic<uint>         AtomicUintT;
typedef std::atomic<long>         AtomicLongT;
typedef std::atomic<ReactorBase*> AtomicReactorBaseT;
typedef std::atomic<IndexT>       AtomicIndexT;

typedef Queue<size_t> SizeQueueT;
typedef Stack<size_t> SizeStackT;

//---------------------------------------------------------
//POD structure
struct ActorStub {
    ActorBase*   pactor;
    ReactorBase* preactor;
    UniqueT      unique;
};

struct ActorChunk {
    ActorChunk(std::mutex& _rmtx)
        : rmtx(_rmtx)
        , svcidx(InvalidIndex())
        , nextchk(InvalidIndex())
        , actcnt(0)
    {
    }
    std::mutex& rmtx;
    size_t      svcidx;
    size_t      nextchk;
    size_t      actcnt;

    void clear()
    {
        svcidx  = InvalidIndex();
        nextchk = InvalidIndex();
        actcnt  = 0;
    }

    char* data()
    {
        return reinterpret_cast<char*>(this);
    }
    ActorStub* actors()
    {
        return reinterpret_cast<ActorStub*>(reinterpret_cast<char*>(this) + sizeof(*this));
    }

    ActorStub& actor(const size_t _idx)
    {
        return actors()[_idx];
    }

    const ActorStub* actors() const
    {
        return reinterpret_cast<const ActorStub*>(reinterpret_cast<const char*>(this) + sizeof(*this));
    }

    ActorStub const& actor(const size_t _idx) const
    {
        return actors()[_idx];
    }
};

enum State {
    StateStartingE = 1,
    StateRunningE,
    StateStoppingE,
    StateStoppedE
};

struct ServiceStub {
    ServiceStub(
        std::mutex& _rmtx)
        : psvc(nullptr)
        , rmtx(_rmtx)
        , firstchk(InvalidIndex())
        , lastchk(InvalidIndex())
        , state(StateStoppedE)
        , crtactidx(InvalidIndex())
        , endactidx(InvalidIndex())
        , actcnt(0)
    {
    }

    void reset(Service* _psvc)
    {
        psvc      = _psvc;
        firstchk  = InvalidIndex();
        lastchk   = InvalidIndex();
        crtactidx = InvalidIndex();
        endactidx = InvalidIndex();
        actcnt    = 0;
        state     = StateRunningE;
        while (!actcache.empty()) {
            actcache.pop();
        }
    }

    void reset()
    {
        solid_dbg(logger, Verbose, "" << psvc);
        psvc      = nullptr;
        firstchk  = InvalidIndex();
        lastchk   = InvalidIndex();
        crtactidx = InvalidIndex();
        endactidx = InvalidIndex();
        actcnt    = 0;
        state     = StateStoppedE;
        while (!actcache.empty()) {
            actcache.pop();
        }
    }

    Service*    psvc;
    std::mutex& rmtx;
    size_t      firstchk;
    size_t      lastchk;
    State       state;
    size_t      crtactidx;
    size_t      endactidx;
    size_t      actcnt;
    SizeStackT  actcache;
};

typedef std::deque<ActorChunk*>   ChunkDequeT;
typedef std::deque<ServiceStub>   ServiceDequeT;
typedef std::vector<ServiceStub*> ServiceVectorT;

struct ServiceStoreStub {
    ServiceStoreStub()
        : nowantwrite(true)
        , usecnt(0)
    {
    }
    AtomicBoolT    nowantwrite;
    AtomicLongT    usecnt;
    ServiceVectorT vec;
};

struct ActorStoreStub {
    ActorStoreStub()
        : nowantwrite(true)
        , usecnt(0)
    {
    }
    AtomicBoolT nowantwrite;
    AtomicLongT usecnt;
    ChunkDequeT vec;
};

typedef std::atomic<State> AtomicStateT;

//---------------------------------------------------------
struct Manager::Data {

    AtomicSizeT             crtsvcstoreidx;
    ServiceDequeT           svcdq;
    ServiceStoreStub        svcstore[2];
    AtomicSizeT             crtsvcidx;
    size_t                  svccnt;
    SizeStackT              svccache;
    std::mutex*             pactmtxarr;
    size_t                  actmtxcnt;
    std::mutex*             psvcmtxarr;
    size_t                  svcmtxcnt;
    AtomicSizeT             crtactstoreidx;
    ActorStoreStub          actstore[2];
    size_t                  actchkcnt;
    AtomicSizeT             maxactcnt;
    SizeStackT              chkcache;
    AtomicStateT            state;
    std::mutex              mtx;
    std::condition_variable cnd;

    Data(
        Manager& _rm);
    ~Data();

    ActorChunk* allocateChunk(std::mutex& _rmtx) const
    {
        char*       p   = new char[sizeof(ActorChunk) + actchkcnt * sizeof(ActorStub)];
        ActorChunk* poc = new (p) ActorChunk(_rmtx);
        for (size_t i = 0; i < actchkcnt; ++i) {
            ActorStub& ras(poc->actor(i));
            ras.pactor   = nullptr;
            ras.preactor = nullptr;
            ras.unique   = 0;
        }
        return poc;
    }

    ActorChunk* chunk(const size_t _storeidx, const size_t _idx) const
    {
        return actstore[_storeidx].vec[_idx / actchkcnt];
    }

    ActorStub& actor(const size_t _storeidx, const size_t _idx)
    {
        return chunk(_storeidx, _idx)->actor(_idx % actchkcnt);
    }

    size_t aquireReadServiceStore()
    {
        size_t idx = crtsvcstoreidx;
        if (svcstore[idx].nowantwrite && svcstore[idx].usecnt.fetch_add(1) >= 0) {
            return idx;
        }
        std::lock_guard<std::mutex> lock(mtx);
        idx    = crtsvcstoreidx;
        long v = svcstore[idx].usecnt.fetch_add(1);
        solid_assert(v >= 0);
        return idx;
    }

    void releaseReadServiceStore(const size_t _idx)
    {
        svcstore[_idx].usecnt.fetch_sub(1);
    }

    size_t aquireWriteServiceStore()
    {
        //NOTE: mtx must be locked
        size_t idx    = (crtsvcstoreidx + 1) % 2;
        long   expect = 0;

        svcstore[idx].nowantwrite = false;

        while (!svcstore[idx].usecnt.compare_exchange_weak(expect, LONG_MIN)) {
            expect = 0;
            std::this_thread::yield();
        }
        return idx;
    }

    size_t releaseWriteServiceStore(const size_t _idx)
    {
        svcstore[_idx].usecnt = 0;
        crtsvcstoreidx        = _idx;
        return _idx;
    }

    size_t aquireReadActorStore()
    {
        size_t idx = crtactstoreidx;
        if (actstore[idx].nowantwrite && actstore[idx].usecnt.fetch_add(1) >= 0) {
            return idx;
        }
        std::lock_guard<std::mutex> lock(mtx);
        idx    = crtactstoreidx;
        long v = actstore[idx].usecnt.fetch_add(1);
        solid_assert(v >= 0);
        return idx;
    }

    void releaseReadActorStore(const size_t _idx)
    {
        actstore[_idx].usecnt.fetch_sub(1);
    }

    size_t aquireWriteActorStore()
    {
        //NOTE: mtx must be locked
        size_t idx    = (crtactstoreidx + 1) % 2;
        long   expect = 0;

        actstore[idx].nowantwrite = false;

        while (!actstore[idx].usecnt.compare_exchange_weak(expect, LONG_MIN)) {
            expect = 0;
            std::this_thread::yield();
        }
        return idx;
    }
    size_t releaseWriteActorStore(const size_t _idx)
    {
        actstore[_idx].usecnt = 0;
        crtactstoreidx        = _idx;
        return _idx;
    }
};

Manager::Data::Data(
    Manager& /*_rm*/)
    : crtsvcstoreidx(0)
    , crtsvcidx(0)
    , svccnt(0)
    , crtactstoreidx(0)
    , maxactcnt(0)
    , state(StateRunningE)
{
}

Manager::Data::~Data()
{
    size_t actstoreidx = 0;
    if (actstore[1].vec.size() > actstore[0].vec.size()) {
        actstoreidx = 1;
    }
    ChunkDequeT& rchdq = actstore[actstoreidx].vec;
    for (auto& v : rchdq) {
        delete[] v->data();
    }
}

bool Manager::VisitContext::raiseActor(Event const& _revt) const
{
    return rr_.raise(ra_.runId(), _revt);
}

bool Manager::VisitContext::raiseActor(Event&& _uevt) const
{
    return rr_.raise(ra_.runId(), std::move(_uevt));
}

Manager::Manager(
    const size_t _svcmtxcnt /* = 0*/,
    const size_t _actmtxcnt /* = 0*/,
    const size_t _actbucketsize /* = 0*/
    )
    : impl_(make_pimpl<Data>(*this))
{
    solid_dbg(logger, Verbose, "" << this);

    if (_actmtxcnt == 0) {
        impl_->actmtxcnt = memory_page_size() / sizeof(std::mutex);
    } else {
        impl_->actmtxcnt = _actmtxcnt;
    }

    if (_svcmtxcnt == 0) {
        impl_->svcmtxcnt = memory_page_size() / sizeof(std::mutex);
    } else {
        impl_->svcmtxcnt = _svcmtxcnt;
    }

    impl_->pactmtxarr = new std::mutex[impl_->actmtxcnt];

    impl_->psvcmtxarr = new std::mutex[impl_->svcmtxcnt];

    if (_actbucketsize == 0) {
        impl_->actchkcnt = (memory_page_size() - sizeof(ActorChunk)) / sizeof(ActorStub);
    } else {
        impl_->actchkcnt = _actbucketsize;
    }
}

/*virtual*/ Manager::~Manager()
{
    solid_dbg(logger, Verbose, "" << this);
    stop();
    solid_dbg(logger, Verbose, "" << this);
    delete[] impl_->pactmtxarr; //TODO: get rid of delete
    delete[] impl_->psvcmtxarr;
}

bool Manager::registerService(
    Service& _rs)
{
    std::lock_guard<std::mutex> lock(impl_->mtx);

    if (_rs.isRegistered()) {
        return false;
    }

    if (impl_->state != StateRunningE) {
        return false;
    }

    size_t crtsvcstoreidx = impl_->crtsvcstoreidx;
    size_t svcidx         = InvalidIndex();

    if (!impl_->svccache.empty()) {

        svcidx = impl_->svccache.top();
        impl_->svccache.top();

    } else if (impl_->crtsvcidx < impl_->svcstore[crtsvcstoreidx].vec.size()) {

        svcidx = impl_->crtsvcidx;
        impl_->svcdq.push_back(ServiceStub(impl_->psvcmtxarr[svcidx % impl_->svcmtxcnt]));
        impl_->svcstore[crtsvcstoreidx].vec[svcidx] = &impl_->svcdq[svcidx];
        ++impl_->crtsvcidx;

    } else {

        const size_t newsvcvecidx = impl_->aquireWriteServiceStore();

        impl_->svcstore[newsvcvecidx].vec = impl_->svcstore[crtsvcstoreidx].vec; //update the new vector

        impl_->svcstore[newsvcvecidx].vec.push_back(nullptr);
        impl_->svcstore[newsvcvecidx].vec.resize(impl_->svcstore[newsvcvecidx].vec.capacity());

        svcidx = impl_->crtsvcidx;

        impl_->svcdq.push_back(ServiceStub(impl_->psvcmtxarr[svcidx % impl_->svcmtxcnt]));
        impl_->svcstore[newsvcvecidx].vec[svcidx] = &impl_->svcdq[svcidx];

        crtsvcstoreidx = impl_->releaseWriteServiceStore(newsvcvecidx);
        ++impl_->crtsvcidx;
    }
    _rs.idx = svcidx;
    ++impl_->svccnt;
    impl_->svcstore[crtsvcstoreidx].vec[svcidx]->reset(&_rs);
    return true;
}

void Manager::unregisterService(Service& _rsvc)
{
    const size_t svcidx = _rsvc.idx;

    if (svcidx == InvalidIndex()) {
        return;
    }

    const size_t svcstoreidx = impl_->aquireReadServiceStore(); //can lock impl_->mtx
    ServiceStub& rss         = *impl_->svcstore[svcstoreidx].vec[svcidx];

    std::lock_guard<std::mutex> lock(rss.rmtx);

    if (rss.psvc != &_rsvc) {
        return;
    }

    impl_->releaseReadServiceStore(svcstoreidx);

    doUnregisterService(rss);
}

void Manager::doUnregisterService(ServiceStub& _rss)
{
    std::lock_guard<std::mutex> lock2(impl_->mtx);

    const size_t actvecidx = impl_->crtactstoreidx; //inside lock, so crtactstoreidx will not change

    impl_->svccache.push(_rss.psvc->idx);

    _rss.psvc->idx.store(InvalidIndex());

    size_t chkidx = _rss.firstchk;

    while (chkidx != InvalidIndex()) {
        ActorChunk* pchk = impl_->actstore[actvecidx].vec[chkidx];
        chkidx           = pchk->nextchk;
        pchk->clear();
        impl_->chkcache.push(chkidx);
    }

    _rss.reset();
    --impl_->svccnt;

    if (impl_->svccnt == 0 && impl_->state == StateStoppingE) {
        impl_->cnd.notify_all();
    }
}

ActorIdT Manager::registerActor(
    const Service&     _rsvc,
    ActorBase&         _ract,
    ReactorBase&       _rr,
    ScheduleFunctionT& _rfct,
    ErrorConditionT&   _rerr)
{
    ActorIdT retval;

    const size_t svcidx = _rsvc.idx;

    if (svcidx == InvalidIndex()) {
        _rerr = error_service_unknown;
        return retval;
    }

    size_t                      actidx      = InvalidIndex();
    const size_t                svcstoreidx = impl_->aquireReadServiceStore(); //can lock impl_->mtx
    ServiceStub&                rss         = *impl_->svcstore[svcstoreidx].vec[svcidx];
    std::lock_guard<std::mutex> lock(rss.rmtx);

    impl_->releaseReadServiceStore(svcstoreidx);

    if (rss.psvc != &_rsvc || rss.state != StateRunningE) {
        _rerr = error_service_not_running;
        return retval;
    }

    if (!rss.actcache.empty()) {

        actidx = rss.actcache.top();
        rss.actcache.pop();

    } else if (rss.crtactidx < rss.endactidx) {

        actidx = rss.crtactidx;
        ++rss.crtactidx;

    } else {

        std::lock_guard<std::mutex> lock2(impl_->mtx);
        size_t                      chkidx = InvalidIndex();
        if (!impl_->chkcache.empty()) {
            chkidx = impl_->chkcache.top();
            impl_->chkcache.pop();
        } else {
            const size_t newactstoreidx = impl_->aquireWriteActorStore();
            const size_t oldactstoreidx = (newactstoreidx + 1) % 2;

            ActorStoreStub& rnewactstore = impl_->actstore[newactstoreidx];

            rnewactstore.vec.insert(
                rnewactstore.vec.end(),
                impl_->actstore[oldactstoreidx].vec.begin() + rnewactstore.vec.size(),
                impl_->actstore[oldactstoreidx].vec.end());
            chkidx = rnewactstore.vec.size();
            rnewactstore.vec.push_back(impl_->allocateChunk(impl_->pactmtxarr[chkidx % impl_->actmtxcnt]));

            impl_->releaseWriteActorStore(newactstoreidx);
        }

        actidx        = chkidx * impl_->actchkcnt;
        rss.crtactidx = actidx + 1;
        rss.endactidx = actidx + impl_->actchkcnt;

        if (impl_->maxactcnt < rss.endactidx) {
            impl_->maxactcnt = rss.endactidx;
        }

        if (rss.firstchk == InvalidIndex()) {
            rss.firstchk = rss.lastchk = chkidx;
        } else {

            //make the link with the last chunk
            const size_t actstoreidx = impl_->crtactstoreidx; //impl_->mtx is locked so it is safe to fetch the value of

            ActorChunk& laschk(*impl_->actstore[actstoreidx].vec[rss.lastchk]);

            std::lock_guard<std::mutex> lock3(laschk.rmtx);

            laschk.nextchk = chkidx;
        }
        rss.lastchk = chkidx;
    }
    {
        const size_t                actstoreidx = impl_->aquireReadActorStore();
        ActorChunk&                 ractchk(*impl_->chunk(actstoreidx, actidx));
        std::lock_guard<std::mutex> lock2(ractchk.rmtx);

        impl_->releaseReadActorStore(actstoreidx);

        if (ractchk.svcidx == InvalidIndex()) {
            ractchk.svcidx = _rsvc.idx;
        }

        solid_assert(ractchk.svcidx == _rsvc.idx);

        _ract.id(actidx);

        if (_rfct(_rr)) {
            //the actor is scheduled
            //NOTE: a scheduler's reactor must ensure that the actor is fully
            //registered onto manager by locking the actor's mutex before calling
            //any the actor's code

            ActorStub& ros = ractchk.actor(actidx % impl_->actchkcnt);

            ros.pactor    = &_ract;
            ros.preactor  = &_rr;
            retval.index  = actidx;
            retval.unique = ros.unique;
            ++ractchk.actcnt;
            ++rss.actcnt;
        } else {
            _ract.id(InvalidIndex());
            //the actor was not scheduled
            rss.actcache.push(actidx);
            _rerr.assign(-3, _rerr.category()); //TODO: proper error
            _rerr = error_actor_schedule;
        }
    }
    return retval;
}

void Manager::unregisterActor(ActorBase& _ract)
{
    size_t svcidx = InvalidIndex();
    size_t actidx = InvalidIndex();
    {
        const size_t                actstoreidx = impl_->aquireReadActorStore();
        ActorChunk&                 ractchk(*impl_->chunk(actstoreidx, static_cast<size_t>(_ract.id())));
        std::lock_guard<std::mutex> lock2(ractchk.rmtx);

        actidx = static_cast<size_t>(_ract.id());

        impl_->releaseReadActorStore(actstoreidx);

        ActorStub& ros = ractchk.actor(_ract.id() % impl_->actchkcnt);

        ros.pactor   = nullptr;
        ros.preactor = nullptr;
        ++ros.unique;

        _ract.id(InvalidIndex());

        --ractchk.actcnt;
        svcidx = ractchk.svcidx;
        solid_assert(svcidx != InvalidIndex());
    }
    {
        solid_assert(actidx != InvalidIndex());

        const size_t                svcstoreidx = impl_->aquireReadServiceStore(); //can lock impl_->mtx
        ServiceStub&                rss         = *impl_->svcstore[svcstoreidx].vec[svcidx];
        std::lock_guard<std::mutex> lock(rss.rmtx);

        impl_->releaseReadServiceStore(svcstoreidx);

        rss.actcache.push(actidx);
        --rss.actcnt;
        solid_dbg(logger, Verbose, "" << this << " serviceid = " << svcidx << " actcnt = " << rss.actcnt);
        if (rss.actcnt == 0 && rss.state == StateStoppingE) {
            impl_->cnd.notify_all();
        }
    }
}

bool Manager::disableActorVisits(ActorBase& _ract)
{
    bool retval = false;
    if (_ract.isRegistered()) {
        const size_t                actstoreidx = impl_->aquireReadActorStore();
        ActorChunk&                 ractchk(*impl_->chunk(actstoreidx, static_cast<size_t>(_ract.id())));
        std::lock_guard<std::mutex> lock(ractchk.rmtx);

        impl_->releaseReadActorStore(actstoreidx);

        ActorStub& ros = ractchk.actor(_ract.id() % impl_->actchkcnt);
        retval         = (ros.preactor != nullptr);
        ros.preactor   = nullptr;
    }
    return retval;
}
#if 1
bool Manager::notify(ActorIdT const& _ruid, Event&& _uevt)
{
    const auto do_notify_fnc = [&_uevt](VisitContext& _rctx) {
        return _rctx.raiseActor(std::move(_uevt));
    };
    
    return doVisit(_ruid, ActorVisitFunctionT{do_notify_fnc});
}
#else
bool Manager::notify(ActorIdT const& _ruid, Event&& _uevt, const size_t _sigmsk /* = 0*/)
{
    bool retval = false;
    if (_ruid.index < impl_->maxactcnt) {
        const size_t actstoreidx = impl_->aquireReadActorStore();
        ActorChunk&  rchk(*impl_->chunk(actstoreidx, _ruid.index));
        {
            std::lock_guard<std::mutex> lock(rchk.rmtx);
            ActorStub const&            ros(impl_->actor(actstoreidx, _ruid.index));

            if (ros.unique == _ruid.unique && ros.pactor && ros.preactor) {
                retval = notify_actor(*ros.pactor, *ros.preactor, std::move(_uevt), _sigmsk);
            }
        }
        impl_->releaseReadActorStore(actstoreidx);
    }
    return retval;
}
#endif

size_t Manager::notifyAll(const Service& _rsvc, Event const& _revt)
{
    const auto do_notify_fnc = [_revt](VisitContext& _rctx) {
        return _rctx.raiseActor(_revt);
    };
    //ActorVisitFunctionT f(std::cref(do_notify_fnc));
    return doForEachServiceActor(_rsvc, ActorVisitFunctionT{do_notify_fnc});
}

bool Manager::doVisit(ActorIdT const& _ruid, const ActorVisitFunctionT _rfct)
{
    bool retval = false;
    if (_ruid.index < impl_->maxactcnt) {
        const size_t actstoreidx = impl_->aquireReadActorStore();
        ActorChunk&  rchk(*impl_->chunk(actstoreidx, static_cast<size_t>(_ruid.index)));
        {
            std::lock_guard<std::mutex> lock(rchk.rmtx);
            ActorStub const&            ros(impl_->actor(actstoreidx, static_cast<size_t>(_ruid.index)));

            if (ros.unique == _ruid.unique && ros.pactor != nullptr && ros.preactor != nullptr) {
                VisitContext ctx(*this, *ros.preactor, *ros.pactor);
                retval = _rfct(ctx);
            }
        }
        impl_->releaseReadActorStore(actstoreidx);
    }
    return retval;
}

bool Manager::raise(const ActorBase& _ract, Event const& _re)
{
    //Current chunk's mutex must be locked

    const size_t     actstoreidx = impl_->aquireReadActorStore();
    ActorStub const& ros(impl_->actor(actstoreidx, static_cast<size_t>(_ract.id())));

    impl_->releaseReadActorStore(actstoreidx);

    solid_assert(ros.pactor == &_ract);

    return ros.preactor->raise(ros.pactor->runId(), _re);
}

std::mutex& Manager::mutex(const ActorBase& _ract) const
{
    std::mutex* pmtx;
    {
        const size_t actstoreidx = impl_->aquireReadActorStore();
        ActorChunk&  rchk(*impl_->chunk(actstoreidx, static_cast<size_t>(_ract.id())));

        pmtx = &rchk.rmtx;
        impl_->releaseReadActorStore(actstoreidx);
    }
    return *pmtx;
}

ActorIdT Manager::id(const ActorBase& _ract) const
{
    const IndexT actidx = _ract.id();
    ActorIdT     retval;

    if (actidx != InvalidIndex()) {
        const size_t     actstoreidx = impl_->aquireReadActorStore();
        ActorStub const& ros(impl_->actor(actstoreidx, static_cast<size_t>(actidx)));
        ActorBase*       pact = ros.pactor;
        solid_assert(pact == &_ract);
        retval = ActorIdT(actidx, ros.unique);
        impl_->releaseReadActorStore(actstoreidx);
    }
    return retval;
}

std::mutex& Manager::mutex(const Service& _rsvc) const
{
    std::mutex*  pmtx        = nullptr;
    const size_t svcidx      = _rsvc.idx.load(/*std::memory_order_seq_cst*/);
    const size_t svcstoreidx = impl_->aquireReadServiceStore(); //can lock impl_->mtx
    ServiceStub& rsvc        = *impl_->svcstore[svcstoreidx].vec[svcidx];

    pmtx = &rsvc.rmtx;

    impl_->releaseReadServiceStore(svcstoreidx);

    return *pmtx;
}

size_t Manager::doForEachServiceActor(const Service& _rsvc, const ActorVisitFunctionT _rfct)
{
    if (!_rsvc.isRegistered()) {
        return 0u;
    }
    const size_t svcidx = _rsvc.idx.load(/*std::memory_order_seq_cst*/);
    size_t       chkidx = InvalidIndex();
    {
        const size_t svcstoreidx = impl_->aquireReadServiceStore(); //can lock impl_->mtx
        {
            ServiceStub&                rss = *impl_->svcstore[svcstoreidx].vec[svcidx];
            std::lock_guard<std::mutex> lock(rss.rmtx);

            chkidx = rss.firstchk;
        }
        impl_->releaseReadServiceStore(svcstoreidx);
    }

    return doForEachServiceActor(chkidx, _rfct);
}

size_t Manager::doForEachServiceActor(const size_t _chkidx, const ActorVisitFunctionT _rfct)
{

    size_t crtchkidx     = _chkidx;
    size_t visited_count = 0;

    while (crtchkidx != InvalidIndex()) {

        const size_t actstoreidx = impl_->aquireReadActorStore(); //can lock impl_->mtx

        ActorChunk& rchk = *impl_->actstore[actstoreidx].vec[crtchkidx];

        std::lock_guard<std::mutex> lock(rchk.rmtx);

        impl_->releaseReadActorStore(actstoreidx);

        ActorStub* poss = rchk.actors();

        for (size_t i(0), cnt(0); i < impl_->actchkcnt && cnt < rchk.actcnt; ++i) {
            if (poss[i].pactor != nullptr && poss[i].preactor != nullptr) {
                VisitContext ctx(*this, *poss[i].preactor, *poss[i].pactor);
                _rfct(ctx);
                ++visited_count;
                ++cnt;
            }
        }

        crtchkidx = rchk.nextchk;
    }

    return visited_count;
}

Service& Manager::service(const ActorBase& _ract) const
{
    Service* psvc   = nullptr;
    size_t   svcidx = InvalidIndex();
    {
        const size_t actstoreidx = impl_->aquireReadActorStore();
        ActorChunk&  rchk(*impl_->chunk(actstoreidx, static_cast<size_t>(_ract.id())));

        std::lock_guard<std::mutex> lock(rchk.rmtx);

        solid_assert(rchk.actor(_ract.id() % impl_->actchkcnt).pactor == &_ract);

        svcidx = rchk.svcidx;
        impl_->releaseReadActorStore(actstoreidx);
    }
    {
        const size_t svcstoreidx = impl_->aquireReadServiceStore();
        psvc                     = impl_->svcstore[svcstoreidx].vec[svcidx]->psvc;
        impl_->releaseReadServiceStore(svcstoreidx);
    }
    return *psvc;
}

bool Manager::startService(Service& _rsvc)
{
    solid_check(_rsvc.isRegistered(), "Service not registered");
    
    const size_t                svcidx      = _rsvc.idx.load(/*std::memory_order_seq_cst*/);
    const size_t                svcstoreidx = impl_->aquireReadServiceStore(); //can lock impl_->mtx
    ServiceStub&                rss         = *impl_->svcstore[svcstoreidx].vec[svcidx];
    std::lock_guard<std::mutex> lock(rss.rmtx);

    impl_->releaseReadServiceStore(svcstoreidx);

    if (rss.state == StateStoppedE) {
        rss.psvc->setRunning();
        rss.state = StateRunningE;
        return true;
    }
    return false;
}

bool Manager::stopService(Service& _rsvc, const bool _wait)
{
    solid_check(_rsvc.isRegistered(), "Service not registered");

    const size_t svcidx      = _rsvc.idx.load(/*std::memory_order_seq_cst*/);
    const size_t svcstoreidx = impl_->aquireReadServiceStore(); //can lock impl_->mtx
    ServiceStub& rss         = *impl_->svcstore[svcstoreidx].vec[svcidx];

    std::unique_lock<std::mutex> lock(rss.rmtx);

    solid_dbg(logger, Verbose, "" << svcidx << " actcnt = " << rss.actcnt);

    impl_->releaseReadServiceStore(svcstoreidx);

    if (rss.state == StateStoppedE) {
        solid_dbg(logger, Verbose, "" << svcidx);
        return false;
    }
    if (rss.state == StateRunningE) {
        const auto raise_lambda = [](VisitContext& _rctx) {
            return _rctx.raiseActor(make_event(GenericEvents::Kill));
        };
        
        rss.psvc->resetRunning();

        const size_t cnt = doForEachServiceActor(rss.firstchk, ActorVisitFunctionT{raise_lambda});

        if (cnt == 0 && rss.actcnt == 0) {
            rss.state = StateStoppedE;
            solid_dbg(logger, Verbose, "StateStoppedE on " << svcidx);
            return false;
        }
        rss.state = StateStoppingE;
    }

    if (rss.state == StateStoppingE && _wait) {
        solid_dbg(logger, Verbose, "" << svcidx);
        while (rss.actcnt != 0u) {
            impl_->cnd.wait(lock);
        }
        solid_dbg(logger, Verbose, "StateStoppedE on " << svcidx);
        rss.state = StateStoppedE;
    }
    return false;
}

void Manager::start()
{
    {
        std::unique_lock<std::mutex> lock(impl_->mtx);

        while (impl_->state != StateRunningE) {
            if (impl_->state == StateStoppedE) {
                impl_->state = StateRunningE;
                continue;
            }
            if (impl_->state == StateRunningE) {
                continue;
            }

            if (impl_->state == StateStoppingE) {
                while (impl_->state == StateStoppingE) {
                    impl_->cnd.wait(lock);
                }
            } else {
                continue;
            }
        }
    }
}

void Manager::stop()
{
    {
        std::unique_lock<std::mutex> lock(impl_->mtx);
        if (impl_->state == StateStoppedE) {
            return;
        }
        if (impl_->state == StateRunningE) {
            impl_->state = StateStoppingE;
        } else if (impl_->state == StateStoppingE) {
            while (impl_->svccnt != 0u) {
                impl_->cnd.wait(lock);
            }
        } else {
            return;
        }
    }

    const size_t svcstoreidx = impl_->aquireReadServiceStore(); //can lock impl_->mtx

    ServiceStoreStub& rsvcstore = impl_->svcstore[svcstoreidx];

    const auto raise_lambda = [](VisitContext& _rctx) {
        return _rctx.raiseActor(make_event(GenericEvents::Kill));
    };

    //broadcast to all actors to stop
    for (auto it = rsvcstore.vec.begin(); it != rsvcstore.vec.end(); ++it) {
        if (*it != nullptr) {
            ServiceStub& rss = *(*it);

            std::lock_guard<std::mutex> lock(rss.rmtx);

            if (rss.psvc != nullptr && rss.state == StateRunningE) {
                rss.psvc->resetRunning();

                const size_t cnt = doForEachServiceActor(rss.firstchk, ActorVisitFunctionT{raise_lambda});
                (void)cnt;
                rss.state = (rss.actcnt != 0) ? StateStoppingE : StateStoppedE;
                solid_dbg(logger, Verbose, "StateStoppedE on " << (it - rsvcstore.vec.begin()));
            }
        }
    }

    //wait for all services to stop
    for (auto it = rsvcstore.vec.begin(); it != rsvcstore.vec.end(); ++it) {
        if (*it != nullptr) {
            ServiceStub& rss = *(*it);

            std::unique_lock<std::mutex> lock(rss.rmtx);

            if (rss.psvc != nullptr && rss.state == StateStoppingE) {
                solid_dbg(logger, Verbose, "wait stop service: " << (it - rsvcstore.vec.begin()));
                while (rss.actcnt != 0u) {
                    impl_->cnd.wait(lock);
                }
                rss.state = StateStoppedE;
                solid_dbg(logger, Verbose, "StateStoppedE on " << (it - rsvcstore.vec.begin()));
            } else if (rss.psvc != nullptr) {
                solid_dbg(logger, Verbose, "unregister already stopped service: " << (it - rsvcstore.vec.begin()) << " " << rss.psvc << " " << rss.state);
                solid_assert(rss.state == StateStoppedE);
            }
        }
    }

    impl_->releaseReadServiceStore(svcstoreidx);

    {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        impl_->state = StateStoppedE;
        impl_->cnd.notify_all();
    }
}
} //namespace frame
} //namespace solid
