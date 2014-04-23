// utility/src/dynamictype.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "system/thread.hpp"
#include "system/debug.hpp"
#include "system/cassert.hpp"
#include "system/mutex.hpp"

#include "system/mutualstore.hpp"
#include "utility/dynamictype.hpp"
#include "utility/dynamicpointer.hpp"

#include <vector>

#ifdef HAS_GNU_ATOMIC
#include <ext/atomicity.h>
#endif

#include "system/atomic.hpp"

namespace solid{


//---------------------------------------------------------------------
//		Shared
//---------------------------------------------------------------------

namespace{

typedef MutualStore<Mutex>	MutexStoreT;

#ifdef HAS_SAFE_STATIC
MutexStoreT &mutexStore(){
	static MutexStoreT		mtxstore(3, 2, 2);
	return mtxstore;
}

size_t specificId(){
	static const size_t id(Thread::specificId());
	return id;
}

#else

MutexStoreT &mutexStoreStub(){
	static MutexStoreT		mtxstore(3, 2, 2);
	return mtxstore;
}

size_t specificIdStub(){
	static const size_t id(Thread::specificId());
	return id;
}


void once_cbk_store(){
	mutexStoreStub();
}

void once_cbk_specific(){
	specificIdStub();
}

MutexStoreT &mutexStore(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_cbk_store, once);
	return mutexStoreStub();
}

size_t specificId(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_cbk_specific, once);
	return specificIdStub();
}
	

#endif

}//namespace


Mutex &shared_mutex_safe(const void *_p){
	Locker<Mutex> lock(Thread::gmutex());
	return mutexStore().safeAt(reinterpret_cast<size_t>(_p));
}
Mutex &shared_mutex(const void *_p){
	return mutexStore().at(reinterpret_cast<size_t>(_p));
}

//---------------------------------------------------------------------
//----	DynamicPointer	----
//---------------------------------------------------------------------

void DynamicPointerBase::clear(DynamicBase *_pdyn){
	cassert(_pdyn);
	if(!_pdyn->release()) delete _pdyn;
}

void DynamicPointerBase::use(DynamicBase *_pdyn){
	_pdyn->use();
}

void DynamicPointerBase::storeSpecific(void *_pv)const{
	Thread::specific(specificId(), _pv);
}

/*static*/ void* DynamicPointerBase::fetchSpecific(){
	return Thread::specific(specificId());
}


//--------------------------------------------------------------------
//		DynamicBase
//--------------------------------------------------------------------

typedef ATOMIC_NS::atomic<size_t>			AtomicSizeT;

/*static*/ size_t DynamicBase::generateId(){
	static AtomicSizeT u(ATOMIC_VAR_INIT(0));
	return u.fetch_add(1/*, ATOMIC_NS::memory_order_seq_cst*/);
}


DynamicBase::~DynamicBase(){}

/*virtual*/ size_t DynamicBase::use(){
	idbgx(Debug::utility, "DynamicBase");
	return 0;
}

//! Used by DynamicPointer to know if the object must be deleted
/*virtual*/ size_t DynamicBase::release(){
	idbgx(Debug::utility, "DynamicBase");
	return 0;
}

/*virtual*/ bool DynamicBase::isTypeDynamic(const size_t _id)const{
	return false;
}
/*virtual*/ size_t DynamicBase::callback(const DynamicMapperBase &_rdm)const{
	return -1;
}

//--------------------------------------------------------------------
//		DynamicMapperBase
//--------------------------------------------------------------------
struct DynamicMapperBase::Data{
	typedef std::vector<GenericFncT>	FncVectorT;
	
	FncVectorT	fncvec;
	Mutex		mtx;
};

bool DynamicMapperBase::check(const size_t _tid)const{
	Locker<Mutex>	lock(d.mtx);
	if(_tid < d.fncvec.size()){
		return d.fncvec[_tid] != NULL;
	}else{
		return false;
	}
}
DynamicMapperBase::DynamicMapperBase():d(*(new Data)){}
DynamicMapperBase::~DynamicMapperBase(){
	delete &d;
}
DynamicMapperBase::GenericFncT DynamicMapperBase::callback(const size_t _tid)const{
	Locker<Mutex>	lock(d.mtx);
	if(_tid < d.fncvec.size()){
		return d.fncvec[_tid];
	}else{
		return NULL;
	}
}
void DynamicMapperBase::callback(const size_t _tid, GenericFncT _pf){
	Locker<Mutex>	lock(d.mtx);
	if(d.fncvec.size() <= _tid){
		d.fncvec.resize(_tid + 1);
	}
	d.fncvec[_tid] = _pf;
}

}//namespace solid
