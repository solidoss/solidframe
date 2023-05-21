// solid/frame/src/sharedstore.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/frame/sharedstore.hpp"
#include "solid/frame/manager.hpp"
#include "solid/frame/reactorcontext.hpp"
#include "solid/frame/service.hpp"
#include "solid/system/cassert.hpp"
#include "solid/system/log.hpp"
#include "solid/system/mutualstore.hpp"
#include "solid/utility/event.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"
#include <atomic>
#include <mutex>

using namespace std;

namespace solid {
namespace frame {
namespace shared {

namespace {
const LoggerT logger("solid::frame::shared");
} // namespace

enum {
    S_RAISE = 1,
};

void PointerBase::doClear(const bool _isalive)
{
    if (psb != nullptr) {
        psb->erasePointer(id(), _isalive);
        uid = UniqueId::invalid();
        psb = nullptr;
    }
}

//---------------------------------------------------------------
//      StoreBase
//---------------------------------------------------------------
typedef std::atomic<size_t> AtomicSizeT;
typedef MutualStore<mutex>  MutexMutualStoreT;
typedef Queue<UniqueId>     UidQueueT;
typedef Stack<size_t>       SizeStackT;
typedef Stack<void*>        VoidPtrStackT;

struct StoreBase::Data {
    Data(
        Manager& _rm)
        : rm(_rm)
        , objmaxcnt{0}
    {
        pfillerasevec = &erasevec[0];
        pconserasevec = &erasevec[1];
    }
    Manager&            rm;
    shared::AtomicSizeT objmaxcnt;
    MutexMutualStoreT   mtxstore;
    UidVectorT          erasevec[2];
    UidVectorT*         pfillerasevec;
    UidVectorT*         pconserasevec;
    SizeVectorT         cacheobjidxvec;
    SizeStackT          cacheobjidxstk;
    ExecWaitVectorT     exewaitvec;
    VoidPtrStackT       cachewaitstk;
    std::mutex          mtx;
};

void StoreBase::Accessor::notify()
{
    store().raise();
}

StoreBase::StoreBase(
    Manager& _rm)
    : impl_(make_pimpl<Data>(_rm))
{
}

/*virtual*/ StoreBase::~StoreBase()
{
}

Manager& StoreBase::manager()
{
    return impl_->rm;
}

mutex& StoreBase::mutex()
{
    return impl_->mtx;
}
mutex& StoreBase::mutex(const size_t _idx)
{
    return impl_->mtxstore.at(_idx);
}

size_t StoreBase::atomicMaxCount() const
{
    return impl_->objmaxcnt.load();
}

UidVectorT& StoreBase::fillEraseVector() const
{
    return *impl_->pfillerasevec;
}

UidVectorT& StoreBase::consumeEraseVector() const
{
    return *impl_->pconserasevec;
}

StoreBase::SizeVectorT& StoreBase::indexVector() const
{
    return impl_->cacheobjidxvec;
}
StoreBase::ExecWaitVectorT& StoreBase::executeWaitVector() const
{
    return impl_->exewaitvec;
}

namespace {

void visit_lock(mutex& _rm)
{
    _rm.lock();
}

void visit_unlock(mutex& _rm)
{
    _rm.unlock();
}

void lock_all(MutexMutualStoreT& _rms, const size_t _sz)
{
    _rms.visit(_sz, visit_lock);
}

void unlock_all(MutexMutualStoreT& _rms, const size_t _sz)
{
    _rms.visit(_sz, visit_unlock);
}

} // namespace

size_t StoreBase::doAllocateIndex()
{
    // mutex is locked
    size_t rv;
    if (!impl_->cacheobjidxstk.empty()) {
        rv = impl_->cacheobjidxstk.top();
        impl_->cacheobjidxstk.pop();
    } else {
        const size_t objcnt = impl_->objmaxcnt.load();
        lock_all(impl_->mtxstore, objcnt);
        doResizeActorVector(objcnt + 1024);
        for (size_t i = objcnt + 1023; i > objcnt; --i) {
            impl_->cacheobjidxstk.push(i);
        }
        impl_->objmaxcnt.store(objcnt + 1024);
        unlock_all(impl_->mtxstore, objcnt);
        rv = objcnt;
        impl_->mtxstore.safeAt(rv);
    }
    return rv;
}
void StoreBase::erasePointer(UniqueId const& _ruid, const bool _isalive)
{
    if (_ruid.index < impl_->objmaxcnt.load()) {
        bool do_notify = true;
        {
            std::lock_guard<std::mutex> lock(mutex(static_cast<size_t>(_ruid.index)));
            do_notify = doDecrementActorUseCount(_ruid, _isalive);
            (void)do_notify;
        }
        notifyActor(_ruid);
    }
}

void StoreBase::notifyActor(UniqueId const& _ruid)
{
    bool do_raise = false;
    {
        std::lock_guard<std::mutex> lock(mutex());
        impl_->pfillerasevec->push_back(_ruid);
        do_raise = impl_->pfillerasevec->size() == 1;
    }
    if (do_raise) {
        manager().notify(manager().id(*this), make_event(GenericEventE::Wake));
    }
}

void StoreBase::raise()
{
    manager().notify(manager().id(*this), make_event(GenericEventE::Wake));
}

/*virtual*/ void StoreBase::onEvent(frame::ReactorContext& _rctx, EventBase&& _revent)
{
    if (_revent == generic_event<GenericEventE::Wake>) {
        {
            std::lock_guard<std::mutex> lock(mutex());
            ulong                       sm = 0;
            if (!impl_->pfillerasevec->empty()) {
                std::exchange(impl_->pconserasevec, impl_->pfillerasevec);
            }
            doExecuteOnSignal(sm);
        }
        solid_log(logger, Verbose, "");
        if (this->doExecute()) {
            this->post(
                _rctx,
                [this](frame::ReactorContext& _rctx, EventBase&& _revent) { onEvent(_rctx, std::move(_revent)); },
                make_event(GenericEventE::Wake));
        }
        impl_->pconserasevec->clear();
    } else if (_revent == generic_event<GenericEventE::Kill>) {
        this->postStop(_rctx);
    }
}

void StoreBase::doCacheActorIndex(const size_t _idx)
{
    impl_->cacheobjidxstk.push(_idx);
}

void StoreBase::doExecuteCache()
{
    solid_log(logger, Verbose, "");
    for (ExecWaitVectorT::const_iterator it(impl_->exewaitvec.begin()); it != impl_->exewaitvec.end(); ++it) {
        impl_->cachewaitstk.push(it->pw);
    }
    impl_->cacheobjidxvec.clear();
    impl_->exewaitvec.clear();
}

void* StoreBase::doTryAllocateWait()
{
    solid_log(logger, Verbose, "");
    if (!impl_->cachewaitstk.empty()) {
        void* rv = impl_->cachewaitstk.top();
        impl_->cachewaitstk.pop();
        return rv;
    }
    return nullptr;
}

} // namespace shared
} // namespace frame
} // namespace solid
