// frame/src/service.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <deque>
#include <vector>
#include <algorithm>
#include <cstring>

#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/exception.hpp"

#include "system/mutualstore.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "frame/manager.hpp"
#include "frame/service.hpp"
#include "frame/object.hpp"
#include "frame/message.hpp"
#include "frame/common.hpp"

namespace solid{
namespace frame{

Service::Service(
	Manager &_rm
):rm(_rm), idx(-1){}

Service::~Service(){
	if(isRegistered()){
		rm.unregisterService(*this);
	}
}

ObjectUidT Service::registerObject(ObjectBase &_robj){
	if(isRegistered()){
		return rm.registerServiceObject(*this, _robj);
	}else{
		return ObjectUidT();
	}
}

namespace{

struct EventNotifier{
	EventNotifier(
		Manager &_rm, SharedEvent const &_revt, const size_t _sigmsk = 0
	):rm(_rm), evt(_revt), sigmsk(_sigmsk){}
	
	Manager			&rm;
	SharedEvent		evt;
	const size_t	sigmsk;
	
	void operator()(ObjectBase &_robj){
		Event tmpevt(evt);
		
		if(!sigmsk || _robj.notify(sigmsk)){
			rm.raise(_robj, tmpevt);
		}
	}
};

}//namespace


bool Service::notifyAll(SharedEvent const & _revt, const size_t _sigmsk = 0){
	if(isRegistered()){
		EventNotifier	notifier(rm, _revt, _sigmsk);
		return rm.forEachServiceObject(*this, notifier);
	}else{
		return false;
	}
}

void Service::reset(){
	if(isRegistered()){
		rm.resetService(*this);
	}
}

void Service::stop(bool _wait){
	if(isRegistered()){
		rm.stopService(*this, _wait);
	}
}
Mutex& Service::mutex()const{
	return rm.serviceMutex(*this);
}

Mutex& Service::mutex(const IndexT &_rfullid)const{
	return rm.mutex(_rfullid);
}
Object* Service::object(const IndexT &_rfullid)const{
	return rm.unsafeObject(_rfullid);
}

ObjectUidT Service::unsafeRegisterObject(ObjectBase &_robj)const{
	const size_t	svcidx = idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	return rm.doUnsafeRegisterServiceObject(svcidx, _robj);
}

void Service::unsafeStop(Locker<Mutex> &_rlock, bool _wait){
	const size_t	svcidx = idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	rm.doWaitStopService(svcidx, _rlock, true);
}
void Service::unsafeReset(Locker<Mutex> &_rlock){
	const size_t	svcidx = idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
	rm.doResetService(svcidx, _rlock);
}

}//namespace frame
}//namespace solid


