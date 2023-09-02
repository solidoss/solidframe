// solid/frame/sharedstore.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/actor.hpp"
#include "solid/frame/common.hpp"
#include "solid/system/error.hpp"
#include "solid/system/pimpl.hpp"
#include "solid/utility/function.hpp"
#include <deque>
#include <mutex>
#include <vector>

namespace solid {
namespace frame {

class ReactorContext;

namespace shared {

struct PointerBase;

typedef std::vector<UniqueId> UidVectorT;

class StoreBase : public Actor {
public:
    typedef shared::UidVectorT UidVectorT;
    struct Accessor {
        StoreBase& store() const
        {
            return rs;
        }
        std::mutex& mutex()
        {
            return rs.mutex();
        }
        UidVectorT& consumeEraseVector() const
        {
            return rs.consumeEraseVector();
        }
        void notify();

    private:
        friend class StoreBase;
        StoreBase& rs;
        Accessor(StoreBase& _rs)
            : rs(_rs)
        {
        }
        Accessor& operator=(const Accessor&);
    };
    Manager& manager();

    virtual ~StoreBase();

protected:
    enum WaitKind {
        UniqueWaitE,
        SharedWaitE,
        ReinitWaitE
    };
    enum StubStates {
        UnlockedStateE = 1,
        UniqueLockStateE,
        SharedLockStateE,
    };

    struct ExecWaitStub {
        ExecWaitStub()
            : pt(nullptr)
            , pw(nullptr)
        {
        }
        ExecWaitStub(UniqueId const& _uid, void* _pt, void* _pw)
            : uid(_uid)
            , pt(_pt)
            , pw(_pw)
        {
        }

        UniqueId uid;
        void*    pt;
        void*    pw;
    };

    typedef std::vector<size_t>       SizeVectorT;
    typedef std::vector<ExecWaitStub> ExecWaitVectorT;

    StoreBase(
        Manager& _rm);

    std::mutex& mutex();
    std::mutex& mutex(const size_t _idx);

    size_t doAllocateIndex();
    void*  doTryAllocateWait();
    void   pointerId(PointerBase& _rpb, UniqueId const& _ruid);
    void   doExecuteCache();
    void   doCacheActorIndex(const size_t _idx);
    size_t atomicMaxCount() const;

    UidVectorT& consumeEraseVector() const;
    UidVectorT& fillEraseVector() const;

    SizeVectorT&     indexVector() const;
    ExecWaitVectorT& executeWaitVector() const;
    Accessor         accessor();
    void             notifyActor(UniqueId const& _ruid);
    void             raise();

private:
    friend struct PointerBase;
    void         erasePointer(UniqueId const& _ruid, const bool _isalive);
    virtual bool doDecrementActorUseCount(UniqueId const& _uid, const bool _isalive) = 0;
    virtual bool doExecute()                                                         = 0;
    virtual void doResizeActorVector(const size_t _newsz)                            = 0;
    virtual void doExecuteOnSignal(ulong _sm)                                        = 0;

    /*virtual*/ void onEvent(frame::ReactorContext& _rctx, EventBase&& _revent) override;

private:
    struct Data;
    Pimpl<Data, 280> impl_;
};

struct PointerBase {
    const UniqueId& id() const
    {
        return uid;
    }
    bool empty() const
    {
        return id().isInvalid();
    }
    StoreBase* store() const
    {
        return psb;
    }
    std::mutex* mutex() const
    {
        if (!empty()) {
            return &psb->mutex(static_cast<size_t>(uid.index));
        } else
            return nullptr;
    }

protected:
    PointerBase(StoreBase* _psb = nullptr)
        : psb(_psb)
    {
    }
    PointerBase(StoreBase* _psb, UniqueId const& _uid)
        : uid(_uid)
        , psb(_psb)
    {
    }
    PointerBase(PointerBase const& _rpb)
        : uid(_rpb.uid)
        , psb(_rpb.psb)
    {
    }

    void doClear(const bool _isalive);
    void doReset(StoreBase* _psb, UniqueId const& _uid) const
    {
        uid = _uid;
        psb = _psb;
    }

private:
    friend class StoreBase;
    mutable UniqueId   uid;
    mutable StoreBase* psb;
};

template <class T>
struct Pointer : PointerBase {

    typedef Pointer<T> PointerT;

    Pointer(StoreBase* _psb = nullptr)
        : PointerBase(_psb)
        , pt(nullptr)
    {
    }
    explicit Pointer(
        T*              _pt,
        StoreBase*      _psb = nullptr,
        UniqueId const& _uid = UniqueId())
        : PointerBase(_psb, _uid)
        , pt(_pt)
    {
    }

    Pointer(PointerT const& _rptr)
        : PointerBase(_rptr)
        , pt(_rptr.release())
    {
    }
    ~Pointer()
    {
        clear();
    }

    PointerT& operator=(PointerT const& _rptr)
    {
        clear();
        doReset(_rptr.store(), _rptr.id());
        pt = _rptr.release();
        return *this;
    }

    void reset(T* _pt, StoreBase* _psb, UniqueId const& _uid)
    {
        clear();
        pt = _pt;
    }
    bool alive() const
    {
        return pt == nullptr;
    }

    T& operator*() const { return *pt; }
    T* operator->() const { return pt; }
    T* get() const { return pt; }
    // operator bool () const    {return psig;}
    bool operator!() const { return empty(); }

    T* release() const
    {
        T* p = pt;
        pt   = nullptr;
        doReset(nullptr, UniqueId());
        return p;
    }
    void clear()
    {
        if (!empty()) {
            doClear(alive());
            pt = nullptr;
        }
    }

private:
    mutable T* pt;
};

enum Flags {
    SynchronousTryFlag = 1
};

template <class T,
    class Ctl>
class Store : public StoreBase {
public:
    typedef Pointer<T> PointerT;
    typedef Ctl        ControllerT;
    typedef StoreBase  BaseT;

    Store(
        Manager& _rm)
        : BaseT(_rm)
    {
    }

    PointerT insertAlive(T& _rt)
    {
        std::lock_guard<std::mutex> lock(this->mutex());
        const size_t                idx = this->doAllocateIndex();
        PointerT                    ptr;
        {
            std::lock_guard<std::mutex> lock2(this->mutex(idx));
            Stub&                       rs = stubvec[idx];
            rs.act                         = _rt;
            ptr                            = doTryGetAlive(idx);
        }
        return ptr;
    }

    PointerT insertShared(T& _rt)
    {
        std::lock_guard<std::mutex> lock(this->mutex());
        const size_t                idx = this->doAllocateIndex();
        PointerT                    ptr;
        {
            std::lock_guard<std::mutex> lock2(this->mutex(idx));
            Stub&                       rs = stubvec[idx];
            rs.act                         = _rt;
            ptr                            = doTryGetShared(idx);
        }
        return ptr;
    }

    PointerT insertUnique(T& _rt)
    {
        std::lock_guard<std::mutex> lock(this->mutex());
        const size_t                idx = this->doAllocateIndex();
        PointerT                    ptr;
        {
            std::lock_guard<std::mutex> lock2(this->mutex(idx));
            Stub&                       rs = stubvec[idx];
            rs.act                         = _rt;
            ptr                            = doTryGetUnique(idx);
        }
        return ptr;
    }

    PointerT insertAlive()
    {
        std::lock_guard<std::mutex> lock(this->mutex());
        const size_t                idx = this->doAllocateIndex();
        PointerT                    ptr;
        {
            std::lock_guard<std::mutex> lock2(this->mutex(idx));
            ptr = doTryGetAlive(idx);
        }
        return ptr;
    }

    PointerT insertShared()
    {
        std::lock_guard<std::mutex> lock(this->mutex());
        const size_t                idx = this->doAllocateIndex();
        PointerT                    ptr;
        {
            std::lock_guard<std::mutex> lock2(this->mutex(idx));
            ptr = doTryGetShared(idx);
        }
        return ptr;
    }

    PointerT insertUnique()
    {
        std::lock_guard<std::mutex> lock(this->mutex());
        const size_t                idx = this->doAllocateIndex();
        PointerT                    ptr;
        {
            std::lock_guard<std::mutex> lock2(this->mutex(idx));
            ptr = doTryGetUnique(idx);
        }
        return ptr;
    }

    // Try get an alive pointer for an intem
    PointerT alive(UniqueId const& _ruid, ErrorCodeT& _rerr)
    {
        PointerT     ptr;
        const size_t idx = _ruid.index;
        if (idx < this->atomicMaxCount()) {
            std::lock_guard<std::mutex> lock2(this->mutex(idx));
            Stub&                       rs = stubvec[idx];
            if (rs.uid == _ruid.unique) {
                ptr = doTryGetAlive(idx);
            }
        }
        return ptr;
    }

    // Try get an unique pointer for an item
    PointerT unique(UniqueId const& _ruid, ErrorCodeT& _rerr)
    {
        PointerT     ptr;
        const size_t idx = _ruid.index;
        if (idx < this->atomicMaxCount()) {
            std::lock_guard<std::mutex> lock2(this->mutex(idx));
            Stub&                       rs = stubvec[idx];
            if (rs.uid == _ruid.unique) {
                ptr = doTryGetUnique(idx);
            }
        }
        return ptr;
    }

    // Try get a shared pointer for an item
    PointerT shared(UniqueId const& _ruid, ErrorCodeT& _rerr)
    {
        PointerT     ptr;
        const size_t idx = _ruid.index;
        if (idx < this->atomicMaxCount()) {
            std::lock_guard<std::mutex> lock2(this->mutex(idx));
            Stub&                       rs = stubvec[idx];
            if (rs.uid == _ruid.unique) {
                ptr = doTryGetShared(idx);
            }
        }
        return ptr;
    }

    bool uniqueToShared(PointerT const& _rptr)
    {
        bool rv        = false;
        bool do_notify = false;
        if (!_rptr.empty()) {
            const size_t idx = _rptr.id().index;
            if (idx < this->atomicMaxCount()) {
                std::lock_guard<std::mutex> lock2(this->mutex(idx));
                Stub&                       rs = stubvec[idx];
                if (rs.uid == _rptr.id().unique) {
                    if (doSwitchUniqueToShared(idx)) {
                        if (rs.pwaitfirst && rs.pwaitfirst->kind == StoreBase::SharedWaitE) {
                            do_notify = true;
                        }
                        rv = true;
                    }
                }
            }
        }
        if (do_notify) {
            StoreBase::notifyActor(_rptr.id());
        }
        return rv;
    }

    //! Return true if the _f was called within the current thread
    template <typename F>
    bool requestReinit(F& _f, size_t _flags = 0)
    {
        PointerT            ptr(this);
        size_t              idx = -1;
        ErrorCodeT          err;
        StoreBase::Accessor acc = StoreBase::accessor();
        {
            std::lock_guard<std::mutex> lock(this->mutex());
            bool                        found = controller().prepareIndex(acc, _f, idx, _flags, err);
            if (!found && !err) {
                // an index was not found - need to allocate one
                idx = this->doAllocateIndex();
            }
            if (!err) {
                std::lock_guard<std::mutex> lock(this->mutex(idx));
                ptr = doTryGetReinit(idx);
                if (ptr.empty()) {
                    doPushWait(idx, _f, StoreBase::ReinitWaitE);
                } else if (!controller().preparePointer(acc, _f, ptr, _flags, err)) {
                    solid_assert_log(ptr.empty(), generic_logger);
                    doPushWait(idx, _f, StoreBase::ReinitWaitE);
                }
            }
        }
        if (!ptr.empty() || err) {
            _f(controller(), ptr, err);
            return true;
        }
        return false;
    }

    //! Return true if the _f was called within the current thread
    template <typename F>
    bool requestShared(F _f, UniqueId const& _ruid, const size_t _flags = 0)
    {
        PointerT     ptr;
        ErrorCodeT   err;
        const size_t idx = _ruid.index;
        if (idx < this->atomicMaxCount()) {
            std::lock_guard<std::mutex> lock2(this->mutex(idx));
            Stub&                       rs = stubvec[idx];
            if (rs.uid == _ruid.unique) {
                ptr = doTryGetShared(idx);
                if (ptr.empty()) {
                    doPushWait(idx, _f, StoreBase::SharedWaitE);
                }
            } else {
                err.assign(1, err.category()); // TODO:
            }
        } else {
            err.assign(1, err.category()); // TODO:
        }
        if (!ptr.empty() || err) {
            _f(controller(), ptr, err);
            return true;
        }
        return false;
    }
    //! Return true if the _f was called within the current thread
    template <typename F>
    bool requestUnique(F _f, UniqueId const& _ruid, const size_t _flags = 0)
    {
        PointerT     ptr;
        ErrorCodeT   err;
        const size_t idx = _ruid.index;
        if (idx < this->atomicMaxCount()) {
            std::lock_guard<std::mutex> lock2(this->mutex(idx));
            Stub&                       rs = stubvec[idx];
            if (rs.uid == _ruid.unique) {
                ptr = doTryGetUnique(idx);
                if (ptr.empty()) {
                    doPushWait(idx, _f, StoreBase::UniqueWaitE);
                }
            } else {
                err.assign(1, err.category()); // TODO:
            }
        } else {
            err.assign(1, err.category()); // TODO:
        }
        if (!ptr.empty() || err) {
            _f(controller(), ptr, err);
            return true;
        }
        return false;
    }
    //! Return true if the _f was called within the current thread
    //_f will be called uniquely when actor's alive count is zero
    template <typename F>
    bool requestReinit(F _f, UniqueId const& _ruid, const size_t _flags = 0)
    {
        PointerT     ptr;
        ErrorCodeT   err;
        const size_t idx = _ruid.index;
        if (idx < this->atomicMaxCount()) {
            std::lock_guard<std::mutex> lock2(this->mutex(idx));
            Stub&                       rs = stubvec[idx];
            if (rs.uid == _ruid.unique) {
                ptr = doTryGetReinit(idx);
                if (ptr.empty()) {
                    doPushWait(idx, _f, StoreBase::ReinitWaitE);
                }
            } else {
                err.assign(1, err.category()); // TODO:
            }
        } else {
            err.assign(1, err.category()); // TODO:
        }
        if (!ptr.empty() || err) {
            _f(controller(), ptr, err);
            return true;
        }
        return false;
    }

    ControllerT& controller()
    {
        return *static_cast<ControllerT*>(this);
    }

private:
    typedef solid_function_t(void(ControllerT&, PointerT&, ErrorCodeT const&)) FunctionT;

    struct WaitStub {
        WaitStub()
            : kind(StoreBase::ReinitWaitE)
            , pnext(nullptr)
        {
        }
        StoreBase::WaitKind kind;
        WaitStub*           pnext;
        FunctionT           fnc;
    };
    struct Stub {
        Stub()
            : uid(0)
            , alivecnt(0)
            , usecnt(0)
            , state(StoreBase::UnlockedStateE)
            , pwaitfirst(nullptr)
            , pwaitlast(nullptr)
        {
        }

        void clear()
        {
            ++uid;
            state = StoreBase::UnlockedStateE;
        }
        bool canClear() const
        {
            return usecnt == 0 && alivecnt == 0 && pwaitfirst == nullptr;
        }

        T         act;
        uint32_t  uid;
        size_t    alivecnt;
        size_t    usecnt;
        uint8_t   state;
        WaitStub* pwaitfirst;
        WaitStub* pwaitlast;
    };

    typedef std::deque<Stub>     StubVectorT;
    typedef std::deque<WaitStub> WaitDequeT;

    /*virtual*/ void doResizeActorVector(const size_t _newsz)
    {
        stubvec.resize(_newsz);
    }

    PointerT doTryGetAlive(const size_t _idx)
    {
        Stub& rs = stubvec[_idx];
        ++rs.alivecnt;
        rs.state = StoreBase::UniqueLockStateE;
        return PointerT(nullptr, this, UniqueId(_idx, rs.uid));
    }

    PointerT doTryGetShared(const size_t _idx)
    {
        Stub& rs = stubvec[_idx];
        if (rs.state == StoreBase::SharedLockStateE && rs.pwaitfirst == nullptr) {
            ++rs.usecnt;
            return PointerT(&rs.act, this, UniqueId(_idx, rs.uid));
        }
        return PointerT();
    }

    PointerT doTryGetUnique(const size_t _idx)
    {
        Stub& rs = stubvec[_idx];
        if (rs.usecnt == 0) {
            solid_assert_log(rs.pwaitfirst == nullptr, generic_logger);
            ++rs.usecnt;
            rs.state = StoreBase::UniqueLockStateE;
            return PointerT(&rs.act, this, UniqueId(_idx, rs.uid));
        }
        return PointerT();
    }
    PointerT doTryGetReinit(const size_t _idx)
    {
        Stub& rs = stubvec[_idx];
        if (rs.usecnt == 0 && rs.alivecnt == 0 && rs.pwaitfirst == nullptr) {
            ++rs.usecnt;
            rs.state = StoreBase::UniqueLockStateE;
            return PointerT(&rs.act, this, UniqueId(_idx, rs.uid));
        }
        return PointerT();
    }
    bool doSwitchUniqueToShared(const size_t _idx)
    {
        Stub& rs = stubvec[_idx];

        if (rs.state == StoreBase::UniqueLockStateE) {
            solid_assert_log(rs.usecnt == 1, generic_logger);
            rs.state = StoreBase::SharedLockStateE;
            return true;
        }
        return false;
    }
    template <typename F>
    void doPushWait(const size_t _idx, F& _f, const StoreBase::WaitKind _k)
    {
        Stub&     rs    = stubvec[_idx];
        WaitStub* pwait = reinterpret_cast<WaitStub*>(this->doTryAllocateWait());
        if (pwait == nullptr) {
            waitdq.push_back(WaitStub());
            pwait = &waitdq.back();
        }
        pwait->kind  = _k;
        pwait->fnc   = std::move(_f);
        pwait->pnext = nullptr;

        if (rs.pwaitlast == nullptr) {
            rs.pwaitfirst = rs.pwaitlast = pwait;
        } else {
            rs.pwaitlast->pnext = pwait;
            rs.pwaitlast        = pwait;
        }
    }

    /*virtual*/ bool doDecrementActorUseCount(UniqueId const& _uid, const bool _isalive)
    {
        // the coresponding mutex is already locked
        Stub& rs = stubvec[_uid.index];
        if (rs.uid == _uid.unique) {
            if (_isalive) {
                --rs.alivecnt;
                return rs.usecnt == 0 && rs.alivecnt == 0;
            } else {
                --rs.usecnt;
                return rs.usecnt == 0;
            }
        }
        return false;
    }

    /*virtual*/ void doExecuteOnSignal(ulong _sm)
    {
        StoreBase::Accessor acc = StoreBase::accessor();
        controller().executeOnSignal(acc, _sm);
    }

    /*virtual*/ bool doExecute()
    {
        StoreBase::Accessor          acc             = StoreBase::accessor();
        StoreBase::UidVectorT const& reraseuidvec    = StoreBase::consumeEraseVector();
        StoreBase::SizeVectorT&      rcacheidxvec    = StoreBase::indexVector();
        StoreBase::ExecWaitVectorT&  rexewaitvec     = StoreBase::executeWaitVector();
        const size_t                 eraseuidvecsize = reraseuidvec.size();
        bool                         must_reschedule = controller().executeBeforeErase(acc);

        if (reraseuidvec.empty()) {
            return must_reschedule;
        }
        std::mutex* pmtx = &this->mutex(reraseuidvec.front().index);
        pmtx->lock();
        for (StoreBase::UidVectorT::const_iterator it(reraseuidvec.begin()); it != reraseuidvec.end(); ++it) {
            std::mutex* ptmpmtx = &this->mutex(it->index);
            if (pmtx != ptmpmtx) {
                pmtx->unlock();
                pmtx = ptmpmtx;
                pmtx->lock();
            }
            Stub& rs = stubvec[it->index];
            if (it->unique == rs.uid) {
                if (static_cast<size_t>(it - reraseuidvec.begin()) >= eraseuidvecsize) {
                    // its an uid added by executeBeforeErase
                    --rs.usecnt;
                }
                if (rs.canClear()) {
                    rcacheidxvec.push_back(it->index);
                } else {
                    doExecuteErase(it->index);
                }
            }
        }
        pmtx->unlock();
        // now execute "waits"
        for (StoreBase::ExecWaitVectorT::const_iterator it(rexewaitvec.begin()); it != rexewaitvec.end(); ++it) {
            PointerT   ptr(reinterpret_cast<T*>(it->pt), this, it->uid);
            WaitStub&  rw = *reinterpret_cast<WaitStub*>(it->pw);
            ErrorCodeT err;
            rw.fnc(this->controller(), ptr, err);
        }

        {
            std::lock_guard<std::mutex> lock(this->mutex());
            if (rcacheidxvec.size()) {
                pmtx = &this->mutex(rcacheidxvec.front());
                pmtx->lock();
                for (StoreBase::SizeVectorT::const_iterator it(rcacheidxvec.begin()); it != rcacheidxvec.end(); ++it) {
                    std::mutex* ptmpmtx = &this->mutex(*it);
                    if (pmtx != ptmpmtx) {
                        pmtx->unlock();
                        pmtx = ptmpmtx;
                        pmtx->lock();
                    }
                    Stub& rs = stubvec[*it];
                    if (rs.canClear()) {
                        rs.clear();
                        must_reschedule = controller().clear(acc, rs.act, *it) || must_reschedule;
                        StoreBase::doCacheActorIndex(*it);
                    }
                }
                pmtx->unlock();
            }
            StoreBase::doExecuteCache();
        }
        return must_reschedule;
    }

    void doExecuteErase(const size_t _idx)
    {
        Stub&                       rs          = stubvec[_idx];
        WaitStub*                   pwait       = rs.pwaitfirst;
        StoreBase::ExecWaitVectorT& rexewaitvec = StoreBase::executeWaitVector();
        while (pwait) {
            switch (pwait->kind) {
            case StoreBase::UniqueWaitE:
                if (rs.usecnt == 0) {
                    // We can deliver
                    rs.state = StoreBase::UniqueLockStateE;
                } else {
                    // cannot deliver right now - keep waiting
                    return;
                }
                break;
            case StoreBase::SharedWaitE:
                if (rs.usecnt == 0 || rs.state == StoreBase::SharedLockStateE) {
                    rs.state = StoreBase::SharedLockStateE;
                } else {
                    // cannot deliver right now - keep waiting
                    return;
                }
                break;
            case StoreBase::ReinitWaitE:
                if (rs.usecnt == 0 && rs.alivecnt == 0) {
                    rs.state = StoreBase::UniqueLockStateE;
                } else {
                    // cannot deliver right now - keep waiting
                    return;
                }
                break;
            default:
                solid_assert_log(false, generic_logger);
                return;
            }
            ++rs.usecnt;
            rexewaitvec.push_back(StoreBase::ExecWaitStub(UniqueId(_idx, rs.uid), &rs.act, pwait));
            if (pwait != rs.pwaitlast) {
                rs.pwaitfirst = pwait->pnext;
            } else {
                rs.pwaitlast = rs.pwaitfirst = nullptr;
            }
            pwait = pwait->pnext;
        }
    }

private:
    StubVectorT stubvec;
    WaitDequeT  waitdq;
};

inline void StoreBase::pointerId(PointerBase& _rpb, UniqueId const& _ruid)
{
    _rpb.uid = _ruid;
}
inline StoreBase::Accessor StoreBase::accessor()
{
    return Accessor(*this);
}
} // namespace shared
} // namespace frame
} // namespace solid
