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
#include "solid/system/debug.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/memory.hpp"
#include <atomic>

#include "solid/frame/manager.hpp"
#include "solid/frame/objectbase.hpp"
#include "solid/frame/reactorbase.hpp"
#include "solid/frame/service.hpp"

#include "solid/utility/event.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"

namespace solid {
namespace frame {

namespace {

enum {
    ErrorServiceUnknownE = 1,
    ErrorServiceNotRunningE,
    ErrorObjectScheduleE
};

class ErrorCategory : public ErrorCategoryT {
public:
    ErrorCategory() {}

    const char* name() const noexcept
    {
        return "solid::frame";
    }

    std::string message(int _ev) const;
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
    case ErrorObjectScheduleE:
        oss << "Object scheduling";
        break;
    default:
        oss << "Unknown";
        break;
    }
    return oss.str();
}
const ErrorConditionT error_service_unknown(ErrorServiceUnknownE, category);
const ErrorConditionT error_service_not_running(ErrorServiceNotRunningE, category);
const ErrorConditionT error_object_schedule(ErrorObjectScheduleE, category);

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
struct ObjectStub {
    ObjectBase*  pobject;
    ReactorBase* preactor;
    UniqueT      unique;
};

struct ObjectChunk {
    ObjectChunk(std::mutex& _rmtx)
        : rmtx(_rmtx)
        , svcidx(InvalidIndex())
        , nextchk(InvalidIndex())
        , objcnt(0)
    {
    }
    std::mutex& rmtx;
    size_t      svcidx;
    size_t      nextchk;
    size_t      objcnt;

    void clear()
    {
        svcidx  = InvalidIndex();
        nextchk = InvalidIndex();
        objcnt  = 0;
    }

    char* data()
    {
        return reinterpret_cast<char*>(this);
    }
    ObjectStub* objects()
    {
        return reinterpret_cast<ObjectStub*>(reinterpret_cast<char*>(this) + sizeof(*this));
    }

    ObjectStub& object(const size_t _idx)
    {
        return objects()[_idx];
    }

    const ObjectStub* objects() const
    {
        return reinterpret_cast<const ObjectStub*>(reinterpret_cast<const char*>(this) + sizeof(*this));
    }

    ObjectStub const& object(const size_t _idx) const
    {
        return objects()[_idx];
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
        , crtobjidx(InvalidIndex())
        , endobjidx(InvalidIndex())
        , objcnt(0)
    {
    }

    void reset(Service* _psvc)
    {
        psvc      = _psvc;
        firstchk  = InvalidIndex();
        lastchk   = InvalidIndex();
        crtobjidx = InvalidIndex();
        endobjidx = InvalidIndex();
        objcnt    = 0;
        state     = StateRunningE;
        while (objcache.size())
            objcache.pop();
    }

    void reset()
    {
        vdbgx(Debug::frame, "" << psvc);
        psvc      = nullptr;
        firstchk  = InvalidIndex();
        lastchk   = InvalidIndex();
        crtobjidx = InvalidIndex();
        endobjidx = InvalidIndex();
        objcnt    = 0;
        state     = StateStoppedE;
        while (objcache.size())
            objcache.pop();
    }

    Service*    psvc;
    std::mutex& rmtx;
    size_t      firstchk;
    size_t      lastchk;
    State       state;
    size_t      crtobjidx;
    size_t      endobjidx;
    size_t      objcnt;
    SizeStackT  objcache;
};

typedef std::deque<ObjectChunk*>  ChunkDequeT;
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

struct ObjectStoreStub {
    ObjectStoreStub()
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
    std::mutex*             pobjmtxarr;
    size_t                  objmtxcnt;
    std::mutex*             psvcmtxarr;
    size_t                  svcmtxcnt;
    AtomicSizeT             crtobjstoreidx;
    ObjectStoreStub         objstore[2];
    size_t                  objchkcnt;
    AtomicSizeT             maxobjcnt;
    SizeStackT              chkcache;
    AtomicStateT            state;
    std::mutex              mtx;
    std::condition_variable cnd;

    Data(
        Manager& _rm);
    ~Data();

    ObjectChunk* allocateChunk(std::mutex& _rmtx) const
    {
        char*        p   = new char[sizeof(ObjectChunk) + objchkcnt * sizeof(ObjectStub)];
        ObjectChunk* poc = new (p) ObjectChunk(_rmtx);
        for (size_t i = 0; i < objchkcnt; ++i) {
            ObjectStub& ros(poc->object(i));
            ros.pobject  = nullptr;
            ros.preactor = nullptr;
            ros.unique   = 0;
        }
        return poc;
    }

    ObjectChunk* chunk(const size_t _storeidx, const size_t _idx) const
    {
        return objstore[_storeidx].vec[_idx / objchkcnt];
    }

    ObjectStub& object(const size_t _storeidx, const size_t _idx)
    {
        return chunk(_storeidx, _idx)->object(_idx % objchkcnt);
    }

    size_t aquireReadServiceStore()
    {
        size_t idx = crtsvcstoreidx;
        if (svcstore[idx].nowantwrite && svcstore[idx].usecnt.fetch_add(1) >= 0) {
            return idx;
        }
        std::unique_lock<std::mutex> lock(mtx);
        idx    = crtsvcstoreidx;
        long v = svcstore[idx].usecnt.fetch_add(1);
        SOLID_ASSERT(v >= 0);
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

    size_t aquireReadObjectStore()
    {
        size_t idx = crtobjstoreidx;
        if (objstore[idx].nowantwrite && objstore[idx].usecnt.fetch_add(1) >= 0) {
            return idx;
        }
        std::unique_lock<std::mutex> lock(mtx);
        idx    = crtobjstoreidx;
        long v = objstore[idx].usecnt.fetch_add(1);
        SOLID_ASSERT(v >= 0);
        return idx;
    }

    void releaseReadObjectStore(const size_t _idx)
    {
        objstore[_idx].usecnt.fetch_sub(1);
    }

    size_t aquireWriteObjectStore()
    {
        //NOTE: mtx must be locked
        size_t idx    = (crtobjstoreidx + 1) % 2;
        long   expect = 0;

        objstore[idx].nowantwrite = false;

        while (!objstore[idx].usecnt.compare_exchange_weak(expect, LONG_MIN)) {
            expect = 0;
            std::this_thread::yield();
        }
        return idx;
    }
    size_t releaseWriteObjectStore(const size_t _idx)
    {
        objstore[_idx].usecnt = 0;
        crtobjstoreidx        = _idx;
        return _idx;
    }
};

Manager::Data::Data(
    Manager& _rm)
    : crtsvcstoreidx(0)
    , crtsvcidx(0)
    , svccnt(0)
    , crtobjstoreidx(0)
    , maxobjcnt(0)
    , state(StateRunningE)
{
}

Manager::Data::~Data()
{
    size_t objstoreidx = 0;
    if (objstore[1].vec.size() > objstore[0].vec.size()) {
        objstoreidx = 1;
    }
    ChunkDequeT& rchdq = objstore[objstoreidx].vec;
    for (auto it = rchdq.begin(); it != rchdq.end(); ++it) {
        delete[](*it)->data();
    }
}

bool Manager::VisitContext::raiseObject(Event const& _revt) const
{
    return rr_.raise(ro_.runId(), _revt);
}

bool Manager::VisitContext::raiseObject(Event&& _uevt) const
{
    return rr_.raise(ro_.runId(), std::move(_uevt));
}

Manager::Manager(
    const size_t _svcmtxcnt /* = 0*/,
    const size_t _objmtxcnt /* = 0*/,
    const size_t _objbucketsize /* = 0*/
    )
    : impl(make_pimpl<Data>(*this))
{
    vdbgx(Debug::frame, "" << this);

    if (_objmtxcnt == 0) {
        impl->objmtxcnt = memory_page_size() / sizeof(std::mutex);
    } else {
        impl->objmtxcnt = _objmtxcnt;
    }

    if (_svcmtxcnt == 0) {
        impl->svcmtxcnt = memory_page_size() / sizeof(std::mutex);
    } else {
        impl->svcmtxcnt = _svcmtxcnt;
    }

    impl->pobjmtxarr = new std::mutex[impl->objmtxcnt];

    impl->psvcmtxarr = new std::mutex[impl->svcmtxcnt];

    if (_objbucketsize == 0) {
        impl->objchkcnt = (memory_page_size() - sizeof(ObjectChunk)) / sizeof(ObjectStub);
    } else {
        impl->objchkcnt = _objbucketsize;
    }
}

/*virtual*/ Manager::~Manager()
{
    vdbgx(Debug::frame, "" << this);
    stop();
    vdbgx(Debug::frame, "" << this);
    delete[] impl->pobjmtxarr; //TODO: get rid of delete
    delete[] impl->psvcmtxarr;
}

bool Manager::registerService(
    Service& _rs)
{
    std::unique_lock<std::mutex> lock(impl->mtx);

    if (_rs.isRegistered()) {
        return false;
    }

    if (impl->state != StateRunningE) {
        return false;
    }

    size_t crtsvcstoreidx = impl->crtsvcstoreidx;
    size_t svcidx         = InvalidIndex();

    if (impl->svccache.size()) {

        svcidx = impl->svccache.top();
        impl->svccache.top();

    } else if (impl->crtsvcidx < impl->svcstore[crtsvcstoreidx].vec.size()) {

        svcidx = impl->crtsvcidx;
        impl->svcdq.push_back(ServiceStub(impl->psvcmtxarr[svcidx % impl->svcmtxcnt]));
        impl->svcstore[crtsvcstoreidx].vec[svcidx] = &impl->svcdq[svcidx];
        ++impl->crtsvcidx;

    } else {

        const size_t newsvcvecidx = impl->aquireWriteServiceStore();

        impl->svcstore[newsvcvecidx].vec = impl->svcstore[crtsvcstoreidx].vec; //update the new vector

        impl->svcstore[newsvcvecidx].vec.push_back(nullptr);
        impl->svcstore[newsvcvecidx].vec.resize(impl->svcstore[newsvcvecidx].vec.capacity());

        svcidx = impl->crtsvcidx;

        impl->svcdq.push_back(ServiceStub(impl->psvcmtxarr[svcidx % impl->svcmtxcnt]));
        impl->svcstore[newsvcvecidx].vec[svcidx] = &impl->svcdq[svcidx];

        crtsvcstoreidx = impl->releaseWriteServiceStore(newsvcvecidx);
        ++impl->crtsvcidx;
    }
    _rs.idx = svcidx;
    ++impl->svccnt;
    impl->svcstore[crtsvcstoreidx].vec[svcidx]->reset(&_rs);
    return true;
}

void Manager::unregisterService(Service& _rsvc)
{
    const size_t svcidx = _rsvc.idx;

    if (svcidx == InvalidIndex()) {
        return;
    }

    const size_t svcstoreidx = impl->aquireReadServiceStore(); //can lock impl->mtx
    ServiceStub& rss         = *impl->svcstore[svcstoreidx].vec[svcidx];

    std::unique_lock<std::mutex> lock(rss.rmtx);

    if (rss.psvc != &_rsvc) {
        return;
    }

    impl->releaseReadServiceStore(svcstoreidx);

    doUnregisterService(rss);
}

void Manager::doUnregisterService(ServiceStub& _rss)
{
    std::unique_lock<std::mutex> lock2(impl->mtx);

    const size_t objvecidx = impl->crtobjstoreidx; //inside lock, so crtobjstoreidx will not change

    impl->svccache.push(_rss.psvc->idx);

    _rss.psvc->idx.store(InvalidIndex());

    size_t chkidx = _rss.firstchk;

    while (chkidx != InvalidIndex()) {
        ObjectChunk* pchk = impl->objstore[objvecidx].vec[chkidx];
        chkidx            = pchk->nextchk;
        pchk->clear();
        impl->chkcache.push(chkidx);
    }

    _rss.reset();
    --impl->svccnt;

    if (impl->svccnt == 0 && impl->state == StateStoppingE) {
        impl->cnd.notify_all();
    }
}

ObjectIdT Manager::registerObject(
    const Service&     _rsvc,
    ObjectBase&        _robj,
    ReactorBase&       _rr,
    ScheduleFunctionT& _rfct,
    ErrorConditionT&   _rerr)
{
    ObjectIdT retval;

    const size_t svcidx = _rsvc.idx;

    if (svcidx == InvalidIndex()) {
        _rerr = error_service_unknown;
        return retval;
    }

    size_t                       objidx      = InvalidIndex();
    const size_t                 svcstoreidx = impl->aquireReadServiceStore(); //can lock impl->mtx
    ServiceStub&                 rss         = *impl->svcstore[svcstoreidx].vec[svcidx];
    std::unique_lock<std::mutex> lock(rss.rmtx);

    impl->releaseReadServiceStore(svcstoreidx);

    if (rss.psvc != &_rsvc || rss.state != StateRunningE) {
        _rerr = error_service_not_running;
        return retval;
    }

    if (rss.objcache.size()) {

        objidx = rss.objcache.top();
        rss.objcache.pop();

    } else if (rss.crtobjidx < rss.endobjidx) {

        objidx = rss.crtobjidx;
        ++rss.crtobjidx;

    } else {

        std::unique_lock<std::mutex> lock2(impl->mtx);
        size_t                       chkidx = InvalidIndex();
        if (impl->chkcache.size()) {
            chkidx = impl->chkcache.top();
            impl->chkcache.pop();
        } else {
            const size_t newobjstoreidx = impl->aquireWriteObjectStore();
            const size_t oldobjstoreidx = (newobjstoreidx + 1) % 2;

            ObjectStoreStub& rnewobjstore = impl->objstore[newobjstoreidx];

            rnewobjstore.vec.insert(
                rnewobjstore.vec.end(),
                impl->objstore[oldobjstoreidx].vec.begin() + rnewobjstore.vec.size(),
                impl->objstore[oldobjstoreidx].vec.end());
            chkidx = rnewobjstore.vec.size();
            rnewobjstore.vec.push_back(impl->allocateChunk(impl->pobjmtxarr[chkidx % impl->objmtxcnt]));

            impl->releaseWriteObjectStore(newobjstoreidx);
        }

        objidx        = chkidx * impl->objchkcnt;
        rss.crtobjidx = objidx + 1;
        rss.endobjidx = objidx + impl->objchkcnt;

        if (impl->maxobjcnt < rss.endobjidx) {
            impl->maxobjcnt = rss.endobjidx;
        }

        if (rss.firstchk == InvalidIndex()) {
            rss.firstchk = rss.lastchk = chkidx;
        } else {

            //make the link with the last chunk
            const size_t objstoreidx = impl->crtobjstoreidx; //impl->mtx is locked so it is safe to fetch the value of

            ObjectChunk& laschk(*impl->objstore[objstoreidx].vec[rss.lastchk]);

            std::unique_lock<std::mutex> lock3(laschk.rmtx);

            laschk.nextchk = chkidx;
        }
        rss.lastchk = chkidx;
    }
    {
        const size_t                 objstoreidx = impl->aquireReadObjectStore();
        ObjectChunk&                 robjchk(*impl->chunk(objstoreidx, objidx));
        std::unique_lock<std::mutex> lock2(robjchk.rmtx);

        impl->releaseReadObjectStore(objstoreidx);

        if (robjchk.svcidx == InvalidIndex()) {
            robjchk.svcidx = _rsvc.idx;
        }

        SOLID_ASSERT(robjchk.svcidx == _rsvc.idx);

        _robj.id(objidx);

        if (_rfct(_rr)) {
            //the object is scheduled
            //NOTE: a scheduler's reactor must ensure that the object is fully
            //registered onto manager by locking the object's mutex before calling
            //any the object's code

            ObjectStub& ros = robjchk.object(objidx % impl->objchkcnt);

            ros.pobject   = &_robj;
            ros.preactor  = &_rr;
            retval.index  = objidx;
            retval.unique = ros.unique;
            ++robjchk.objcnt;
            ++rss.objcnt;
        } else {
            _robj.id(InvalidIndex());
            //the object was not scheduled
            rss.objcache.push(objidx);
            _rerr.assign(-3, _rerr.category()); //TODO: proper error
            _rerr = error_object_schedule;
        }
    }
    return retval;
}

void Manager::unregisterObject(ObjectBase& _robj)
{
    size_t svcidx = InvalidIndex();
    size_t objidx = InvalidIndex();
    {
        const size_t                 objstoreidx = impl->aquireReadObjectStore();
        ObjectChunk&                 robjchk(*impl->chunk(objstoreidx, _robj.id()));
        std::unique_lock<std::mutex> lock2(robjchk.rmtx);

        objidx = _robj.id();

        impl->releaseReadObjectStore(objstoreidx);

        ObjectStub& ros = robjchk.object(_robj.id() % impl->objchkcnt);

        ros.pobject  = nullptr;
        ros.preactor = nullptr;
        ++ros.unique;

        _robj.id(InvalidIndex());

        --robjchk.objcnt;
        svcidx = robjchk.svcidx;
        SOLID_ASSERT(svcidx != InvalidIndex());
    }
    {
        SOLID_ASSERT(objidx != InvalidIndex());

        const size_t                 svcstoreidx = impl->aquireReadServiceStore(); //can lock impl->mtx
        ServiceStub&                 rss         = *impl->svcstore[svcstoreidx].vec[svcidx];
        std::unique_lock<std::mutex> lock(rss.rmtx);

        impl->releaseReadServiceStore(svcstoreidx);

        rss.objcache.push(objidx);
        --rss.objcnt;
        vdbgx(Debug::frame, "" << this << " serviceid = " << svcidx << " objcnt = " << rss.objcnt);
        if (rss.objcnt == 0 && rss.state == StateStoppingE) {
            impl->cnd.notify_all();
        }
    }
}

bool Manager::disableObjectVisits(ObjectBase& _robj)
{
    bool retval = false;
    if (_robj.isRegistered()) {
        const size_t                 objstoreidx = impl->aquireReadObjectStore();
        ObjectChunk&                 robjchk(*impl->chunk(objstoreidx, _robj.id()));
        std::unique_lock<std::mutex> lock(robjchk.rmtx);

        impl->releaseReadObjectStore(objstoreidx);

        ObjectStub& ros = robjchk.object(_robj.id() % impl->objchkcnt);
        retval          = (ros.preactor != nullptr);
        ros.preactor    = nullptr;
    }
    return retval;
}
#if 1
bool Manager::notify(ObjectIdT const& _ruid, Event&& _uevt)
{
    const auto do_notify_fnc = [&_uevt](VisitContext& _rctx) {
        return _rctx.raiseObject(std::move(_uevt));
    };
    //ObjectVisitFunctionT f(std::cref(do_notify_fnc));

    return doVisit(_ruid, ObjectVisitFunctionT{do_notify_fnc});
}
#else
bool Manager::notify(ObjectIdT const& _ruid, Event&& _uevt, const size_t _sigmsk /* = 0*/)
{
    bool retval = false;
    if (_ruid.index < impl->maxobjcnt) {
        const size_t objstoreidx = impl->aquireReadObjectStore();
        ObjectChunk& rchk(*impl->chunk(objstoreidx, _ruid.index));
        {
            std::unique_lock<std::mutex> lock(rchk.rmtx);
            ObjectStub const&            ros(impl->object(objstoreidx, _ruid.index));

            if (ros.unique == _ruid.unique && ros.pobject && ros.preactor) {
                retval = notify_object(*ros.pobject, *ros.preactor, std::move(_uevt), _sigmsk);
            }
        }
        impl->releaseReadObjectStore(objstoreidx);
    }
    return retval;
}
#endif

size_t Manager::notifyAll(const Service& _rsvc, Event const& _revt)
{
    const auto do_notify_fnc = [_revt](VisitContext& _rctx) {
        return _rctx.raiseObject(_revt);
    };
    //ObjectVisitFunctionT f(std::cref(do_notify_fnc));
    return doForEachServiceObject(_rsvc, ObjectVisitFunctionT{do_notify_fnc});
}

bool Manager::doVisit(ObjectIdT const& _ruid, const ObjectVisitFunctionT _rfct)
{
    bool retval = false;
    if (_ruid.index < impl->maxobjcnt) {
        const size_t objstoreidx = impl->aquireReadObjectStore();
        ObjectChunk& rchk(*impl->chunk(objstoreidx, _ruid.index));
        {
            std::unique_lock<std::mutex> lock(rchk.rmtx);
            ObjectStub const&            ros(impl->object(objstoreidx, _ruid.index));

            if (ros.unique == _ruid.unique && ros.pobject && ros.preactor) {
                VisitContext ctx(*this, *ros.preactor, *ros.pobject);
                retval = _rfct(ctx);
            }
        }
        impl->releaseReadObjectStore(objstoreidx);
    }
    return retval;
}

bool Manager::raise(const ObjectBase& _robj, Event const& _re)
{
    //Current chunk's mutex must be locked

    const size_t      objstoreidx = impl->aquireReadObjectStore();
    ObjectStub const& ros(impl->object(objstoreidx, _robj.id()));

    impl->releaseReadObjectStore(objstoreidx);

    SOLID_ASSERT(ros.pobject == &_robj);

    return ros.preactor->raise(ros.pobject->runId(), _re);
}

std::mutex& Manager::mutex(const ObjectBase& _robj) const
{
    std::mutex* pmtx;
    {
        const size_t objstoreidx = impl->aquireReadObjectStore();
        ObjectChunk& rchk(*impl->chunk(objstoreidx, _robj.id()));

        pmtx = &rchk.rmtx;
        impl->releaseReadObjectStore(objstoreidx);
    }
    return *pmtx;
}

ObjectIdT Manager::id(const ObjectBase& _robj) const
{
    const IndexT objidx = _robj.id();
    ObjectIdT    retval;

    if (objidx != InvalidIndex()) {
        const size_t      objstoreidx = impl->aquireReadObjectStore();
        ObjectStub const& ros(impl->object(objstoreidx, objidx));
        ObjectBase*       pobj = ros.pobject;
        SOLID_ASSERT(pobj == &_robj);
        retval = ObjectIdT(objidx, ros.unique);
        impl->releaseReadObjectStore(objstoreidx);
    }
    return retval;
}

std::mutex& Manager::mutex(const Service& _rsvc) const
{
    std::mutex*  pmtx        = nullptr;
    const size_t svcidx      = _rsvc.idx.load(/*std::memory_order_seq_cst*/);
    const size_t svcstoreidx = impl->aquireReadServiceStore(); //can lock impl->mtx
    ServiceStub& rsvc        = *impl->svcstore[svcstoreidx].vec[svcidx];

    pmtx = &rsvc.rmtx;

    impl->releaseReadServiceStore(svcstoreidx);

    return *pmtx;
}

size_t Manager::doForEachServiceObject(const Service& _rsvc, const ObjectVisitFunctionT _rfct)
{
    if (!_rsvc.isRegistered()) {
        return false;
    }
    const size_t svcidx = _rsvc.idx.load(/*std::memory_order_seq_cst*/);
    size_t       chkidx = InvalidIndex();
    {
        const size_t svcstoreidx = impl->aquireReadServiceStore(); //can lock impl->mtx
        {
            ServiceStub&                 rss = *impl->svcstore[svcstoreidx].vec[svcidx];
            std::unique_lock<std::mutex> lock(rss.rmtx);

            chkidx = rss.firstchk;
        }
        impl->releaseReadServiceStore(svcstoreidx);
    }

    return doForEachServiceObject(chkidx, _rfct);
}

size_t Manager::doForEachServiceObject(const size_t _chkidx, const ObjectVisitFunctionT _rfct)
{

    size_t crtchkidx     = _chkidx;
    size_t visited_count = 0;

    while (crtchkidx != InvalidIndex()) {

        const size_t objstoreidx = impl->aquireReadObjectStore(); //can lock impl->mtx

        ObjectChunk& rchk = *impl->objstore[objstoreidx].vec[crtchkidx];

        std::unique_lock<std::mutex> lock(rchk.rmtx);

        impl->releaseReadObjectStore(objstoreidx);

        ObjectStub* poss = rchk.objects();

        for (size_t i(0), cnt(0); i < impl->objchkcnt && cnt < rchk.objcnt; ++i) {
            if (poss[i].pobject && poss[i].preactor) {
                VisitContext ctx(*this, *poss[i].preactor, *poss[i].pobject);
                _rfct(ctx);
                ++visited_count;
                ++cnt;
            }
        }

        crtchkidx = rchk.nextchk;
    }

    return visited_count;
}

Service& Manager::service(const ObjectBase& _robj) const
{
    Service* psvc   = nullptr;
    size_t   svcidx = InvalidIndex();
    {
        const size_t objstoreidx = impl->aquireReadObjectStore();
        ObjectChunk& rchk(*impl->chunk(objstoreidx, _robj.id()));

        std::unique_lock<std::mutex> lock(rchk.rmtx);

        SOLID_ASSERT(rchk.object(_robj.id() % impl->objchkcnt).pobject == &_robj);

        svcidx = rchk.svcidx;
        impl->releaseReadObjectStore(objstoreidx);
    }
    {
        const size_t svcstoreidx = impl->aquireReadServiceStore();
        psvc                     = impl->svcstore[svcstoreidx].vec[svcidx]->psvc;
        impl->releaseReadServiceStore(svcstoreidx);
    }
    return *psvc;
}

bool Manager::startService(Service& _rsvc)
{
    if (!_rsvc.isRegistered()) {
        return false;
    }
    const size_t svcidx      = _rsvc.idx.load(/*std::memory_order_seq_cst*/);
    const size_t svcstoreidx = impl->aquireReadServiceStore(); //can lock impl->mtx
    ServiceStub& rss         = *impl->svcstore[svcstoreidx].vec[svcidx];

    std::unique_lock<std::mutex> lock(rss.rmtx);

    impl->releaseReadServiceStore(svcstoreidx);

    if (rss.state == StateStoppedE) {
        rss.psvc->setRunning();
        rss.state = StateRunningE;
        return true;
    }
    return false;
}

void Manager::stopService(Service& _rsvc, const bool _wait)
{
    if (!_rsvc.isRegistered()) {
        return;
    }

    const size_t svcidx      = _rsvc.idx.load(/*std::memory_order_seq_cst*/);
    const size_t svcstoreidx = impl->aquireReadServiceStore(); //can lock impl->mtx
    ServiceStub& rss         = *impl->svcstore[svcstoreidx].vec[svcidx];

    std::unique_lock<std::mutex> lock(rss.rmtx);

    vdbgx(Debug::frame, "" << svcidx << " objcnt = " << rss.objcnt);

    impl->releaseReadServiceStore(svcstoreidx);

    if (rss.state == StateStoppedE) {
        vdbgx(Debug::frame, "" << svcidx);
        return;
    }
    if (rss.state == StateRunningE) {
        const auto raise_lambda = [](VisitContext& _rctx) {
            return _rctx.raiseObject(make_event(GenericEvents::Kill));
        };
        //ObjectVisitFunctionT fctor(std::cref(l));

        rss.psvc->resetRunning();

        const bool cnt = doForEachServiceObject(rss.firstchk, ObjectVisitFunctionT{raise_lambda});

        if (cnt == 0 and rss.objcnt == 0) {
            rss.state = StateStoppedE;
            vdbgx(Debug::frame, "StateStoppedE on " << svcidx);
            return;
        }
        rss.state = StateStoppingE;
    }

    if (rss.state == StateStoppingE && _wait) {
        vdbgx(Debug::frame, "" << svcidx);
        while (rss.objcnt) {
            impl->cnd.wait(lock);
        }
        vdbgx(Debug::frame, "StateStoppedE on " << svcidx);
        rss.state = StateStoppedE;
    }
}

void Manager::start()
{
    {
        std::unique_lock<std::mutex> lock(impl->mtx);

        while (impl->state != StateRunningE) {
            if (impl->state == StateStoppedE) {
                impl->state = StateRunningE;
                continue;
            }
            if (impl->state == StateRunningE) {
                continue;
            } else if (impl->state == StateStoppingE) {
                while (impl->state == StateStoppingE) {
                    impl->cnd.wait(lock);
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
        std::unique_lock<std::mutex> lock(impl->mtx);
        if (impl->state == StateStoppedE) {
            return;
        }
        if (impl->state == StateRunningE) {
            impl->state = StateStoppingE;
        } else if (impl->state == StateStoppingE) {
            while (impl->svccnt) {
                impl->cnd.wait(lock);
            }
        } else {
            return;
        }
    }

    const size_t svcstoreidx = impl->aquireReadServiceStore(); //can lock impl->mtx

    ServiceStoreStub& rsvcstore = impl->svcstore[svcstoreidx];

    const auto raise_lambda = [](VisitContext& _rctx) {
        return _rctx.raiseObject(make_event(GenericEvents::Kill));
    };

    //broadcast to all objects to stop
    for (auto it = rsvcstore.vec.begin(); it != rsvcstore.vec.end(); ++it) {
        if (*it) {
            ServiceStub& rss = *(*it);

            std::unique_lock<std::mutex> lock(rss.rmtx);

            if (rss.psvc && rss.state == StateRunningE) {
                rss.psvc->resetRunning();

                const size_t cnt = doForEachServiceObject(rss.firstchk, ObjectVisitFunctionT{raise_lambda});
                (void)cnt;
                rss.state = (rss.objcnt != 0) ? StateStoppingE : StateStoppedE;
                vdbgx(Debug::frame, "StateStoppedE on " << (it - rsvcstore.vec.begin()));
            }
        }
    }

    //wait for all services to stop
    for (auto it = rsvcstore.vec.begin(); it != rsvcstore.vec.end(); ++it) {
        if (*it) {
            ServiceStub& rss = *(*it);

            std::unique_lock<std::mutex> lock(rss.rmtx);

            if (rss.psvc && rss.state == StateStoppingE) {
                vdbgx(Debug::frame, "wait stop service: " << (it - rsvcstore.vec.begin()));
                while (rss.objcnt) {
                    impl->cnd.wait(lock);
                }
                rss.state = StateStoppedE;
                vdbgx(Debug::frame, "StateStoppedE on " << (it - rsvcstore.vec.begin()));
            } else if (rss.psvc) {
                vdbgx(Debug::frame, "unregister already stopped service: " << (it - rsvcstore.vec.begin()) << " " << rss.psvc << " " << rss.state);
                SOLID_ASSERT(rss.state == StateStoppedE);
            }
        }
    }

    impl->releaseReadServiceStore(svcstoreidx);

    {
        std::unique_lock<std::mutex> lock(impl->mtx);
        impl->state = StateStoppedE;
        impl->cnd.notify_all();
    }
}
} //namespace frame
} //namespace solid
