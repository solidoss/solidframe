// frame/src/sharedstore.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/sharedstore.hpp"
#include "frame/manager.hpp"
#include "system/cassert.hpp"
#include "system/mutualstore.hpp"
#include "system/atomic.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"


namespace solid{
namespace frame{
namespace shared{

void PointerBase::doClear(const bool _isalive){
	if(psb){
		psb->erasePointer(id(), _isalive);
		uid = invalid_uid();
		psb = NULL;
	}
}

//---------------------------------------------------------------
//		StoreBase
//---------------------------------------------------------------
typedef ATOMIC_NS::atomic<size_t>			AtomicSizeT;
typedef MutualStore<Mutex>					MutexMutualStoreT;
typedef Queue<UidT>							UidQueueT;
typedef Stack<size_t>						SizeStackT;
typedef Stack<void*>						VoidPtrStackT;


struct StoreBase::Data{
	Data(Manager &_rm):rm(_rm), objmaxcnt(ATOMIC_VAR_INIT(0)){
		pfillerasevec = &erasevec[0];
		pconserasevec = &erasevec[1];
	}
	Manager				&rm;
	AtomicSizeT			objmaxcnt;
	MutexMutualStoreT	mtxstore;
	UidVectorT			erasevec[2];
	UidVectorT			*pfillerasevec;
	UidVectorT			*pconserasevec;
	SizeVectorT			cacheobjidxvec;
	SizeStackT			cacheobjidxstk;
	ExecWaitVectorT		exewaitvec;
	VoidPtrStackT		cachewaitstk;
};


void StoreBase::Accessor::notify(size_t _evt){
	if(store().notify(_sm)){
		store().manager().raise(store());
	}
}

StoreBase::StoreBase(Manager &_rm):d(*(new Data(_rm))){}

/*virtual*/ StoreBase::~StoreBase(){
	delete &d;
}


Manager& StoreBase::manager(){
	return d.rm;
}

Mutex &StoreBase::mutex(){
	return manager().mutex(*this);
}
Mutex &StoreBase::mutex(const size_t _idx){
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

	void visit_lock(Mutex &_rm){
		_rm.lock();
	}

	void visit_unlock(Mutex &_rm){
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
	size_t		rv;
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
void StoreBase::erasePointer(UidT const & _ruid, const bool _isalive){
	if(_ruid.index < d.objmaxcnt.load()){
		bool	do_notify = true;
		{
			Locker<Mutex>	lock(mutex(_ruid.index));
			do_notify = doDecrementObjectUseCount(_ruid, _isalive);
		}
		notifyObject(_ruid);
	}
}

void StoreBase::notifyObject(UidT const & _ruid){
	bool	do_raise = false;
	{
		Locker<Mutex>	lock(mutex());
		d.pfillerasevec->push_back(_ruid);
		if(d.pfillerasevec->size() == 1){
			do_raise = Object::notify(frame::S_SIG | frame::S_RAISE);
		}
	}
	if(do_raise){
		manager().raise(*this);
	}
}

/*virtual*/ void StoreBase::execute(ExecuteContext &_rexectx){
	vdbgx(Debug::frame, "");
	if(notified()){
		Locker<Mutex>	lock(mutex());
		ulong sm = grabSignalMask();
		if(sm & frame::S_KILL){
			_rexectx.close();
			return;
		}
		if(sm & frame::S_RAISE){
			if(d.pfillerasevec->size()){
				solid::exchange(d.pconserasevec, d.pfillerasevec);
			}
			doExecuteOnSignal(sm);
		}
	}
	vdbgx(Debug::frame, "");
	if(this->doExecute()){
		_rexectx.reschedule();
	}
	d.pconserasevec->clear();
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
	return NULL;
}

}//namespace shared
}//namespace frame
}//namespace solid
