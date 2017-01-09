// solid/frame/src/sharedstore.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <mutex>
#include "solid/frame/sharedstore.hpp"
#include "solid/frame/manager.hpp"
#include "solid/system/cassert.hpp"
#include "solid/system/mutualstore.hpp"
#include <atomic>
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"
#include "solid/utility/event.hpp"
#include "solid/frame/reactorcontext.hpp"
#include "solid/frame/service.hpp"

using namespace std;

namespace solid{
namespace frame{
namespace shared{

enum{
    S_RAISE = 1,
};

void PointerBase::doClear(const bool _isalive){
    if(psb){
        psb->erasePointer(id(), _isalive);
        uid = UniqueId::invalid();
        psb = nullptr;
    }
}

//---------------------------------------------------------------
//      StoreBase
//---------------------------------------------------------------
typedef std::atomic<size_t>         AtomicSizeT;
typedef MutualStore<mutex>                  MutexMutualStoreT;
typedef Queue<UniqueId>                     UidQueueT;
typedef Stack<size_t>                       SizeStackT;
typedef Stack<void*>                        VoidPtrStackT;


struct StoreBase::Data{
    Data(
        Manager &_rm
    ):  rm(_rm),
        objmaxcnt(ATOMIC_VAR_INIT(0))
    {
        pfillerasevec = &erasevec[0];
        pconserasevec = &erasevec[1];
    }
    Manager             &rm;
    shared::AtomicSizeT objmaxcnt;
    MutexMutualStoreT   mtxstore;
    UidVectorT          erasevec[2];
    UidVectorT          *pfillerasevec;
    UidVectorT          *pconserasevec;
    SizeVectorT         cacheobjidxvec;
    SizeStackT          cacheobjidxstk;
    ExecWaitVectorT     exewaitvec;
    VoidPtrStackT       cachewaitstk;
    std::mutex          mtx;
};


void StoreBase::Accessor::notify(){
    store().raise();
}

StoreBase::StoreBase(
    Manager &_rm
):d(*(new Data(_rm))){}

/*virtual*/ StoreBase::~StoreBase(){
    delete &d;
}


Manager& StoreBase::manager(){
    return d.rm;
}

mutex &StoreBase::mutex(){
    return d.mtx;
}
mutex &StoreBase::mutex(const size_t _idx){
    return d.mtxstore.at(_idx);
}

size_t StoreBase::atomicMaxCount()const{
    return d.objmaxcnt.load();
}

UidVectorT& StoreBase::fillEraseVector()const{
    return *d.pfillerasevec;
}

UidVectorT& StoreBase::consumeEraseVector()const{
    return *d.pconserasevec;
}

StoreBase::SizeVectorT& StoreBase::indexVector()const{
    return d.cacheobjidxvec;
}
StoreBase::ExecWaitVectorT& StoreBase::executeWaitVector()const{
    return d.exewaitvec;
}


namespace{

    void visit_lock(mutex &_rm){
        _rm.lock();
    }

    void visit_unlock(mutex &_rm){
        _rm.unlock();
    }

    void lock_all(MutexMutualStoreT &_rms, const size_t _sz){
        _rms.visit(_sz, visit_lock);
    }

    void unlock_all(MutexMutualStoreT &_rms, const size_t _sz){
        _rms.visit(_sz, visit_unlock);
    }

}//namespace

size_t StoreBase::doAllocateIndex(){
    //mutex is locked
    size_t      rv;
    if(d.cacheobjidxstk.size()){
        rv = d.cacheobjidxstk.top();
        d.cacheobjidxstk.pop();
    }else{
        const size_t objcnt = d.objmaxcnt.load();
        lock_all(d.mtxstore, objcnt);
        doResizeObjectVector(objcnt + 1024);
        for(size_t i = objcnt + 1023; i > objcnt; --i){
            d.cacheobjidxstk.push(i);
        }
        d.objmaxcnt.store(objcnt + 1024);
        unlock_all(d.mtxstore, objcnt);
        rv = objcnt;
        d.mtxstore.safeAt(rv);
    }
    return rv;
}
void StoreBase::erasePointer(UniqueId const & _ruid, const bool _isalive){
    if(_ruid.index < d.objmaxcnt.load()){
        bool    do_notify = true;
        {
            std::unique_lock<std::mutex>    lock(mutex(_ruid.index));
            do_notify = doDecrementObjectUseCount(_ruid, _isalive);
            (void)do_notify;
        }
        notifyObject(_ruid);
    }
}

void StoreBase::notifyObject(UniqueId const & _ruid){
    bool    do_raise = false;
    {
        std::unique_lock<std::mutex>    lock(mutex());
        d.pfillerasevec->push_back(_ruid);
        if(d.pfillerasevec->size() == 1){
            do_raise = Object::notify(S_RAISE);
        }
    }
    if(do_raise){
        manager().notify(manager().id(*this), make_event(GenericEvents::Raise));
    }
}

void StoreBase::raise(){
    manager().notify(manager().id(*this), make_event(GenericEvents::Raise));
}


/*virtual*/void StoreBase::onEvent(frame::ReactorContext &_rctx, Event &&_revent){
    if(_revent == generic_event_raise){
        {
            std::unique_lock<std::mutex>    lock(mutex());
            ulong sm = grabSignalMask();
            if(sm & S_RAISE){
                if(d.pfillerasevec->size()){
                    solid::exchange(d.pconserasevec, d.pfillerasevec);
                }
                doExecuteOnSignal(sm);
            }
        }
        vdbgx(Debug::frame, "");
        if(this->doExecute()){
            this->post(
                _rctx,
                [this](frame::ReactorContext &_rctx, Event &&_revent){onEvent(_rctx, std::move(_revent));},
                make_event(GenericEvents::Raise)
            );
        }
        d.pconserasevec->clear();
    }else if(_revent == generic_event_kill){
        this->postStop(_rctx);
    }
}

void StoreBase::doCacheObjectIndex(const size_t _idx){
    d.cacheobjidxstk.push(_idx);
}

void StoreBase::doExecuteCache(){
    vdbgx(Debug::frame, "");
    for(ExecWaitVectorT::const_iterator it(d.exewaitvec.begin()); it != d.exewaitvec.end(); ++it){
        d.cachewaitstk.push(it->pw);
    }
    d.cacheobjidxvec.clear();
    d.exewaitvec.clear();
}

void* StoreBase::doTryAllocateWait(){
    vdbgx(Debug::frame, "");
    if(d.cachewaitstk.size()){
        void *rv = d.cachewaitstk.top();
        d.cachewaitstk.pop();
        return rv;
    }
    return nullptr;
}

}//namespace shared
}//namespace frame
}//namespace solid
