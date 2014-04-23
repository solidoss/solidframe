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

ObjectUidT Service::registerObject(Object &_robj){
	if(isRegistered()){
		return rm.registerServiceObject(*this, _robj);
	}else{
		return ObjectUidT();
	}
}

namespace{

struct SignalNotifier{
	SignalNotifier(Manager &_rm, ulong _sm):rm(_rm), sm(_sm){}
	Manager	&rm;
	ulong	sm;
	
	void operator()(Object &_robj){
		if(_robj.notify(sm)){
			rm.raise(_robj);
		}
	}
};

struct MessageNotifier{
	MessageNotifier(Manager &_rm, MessageSharedPointerT &_rmsgptr):rm(_rm), rmsgptr(_rmsgptr){}
	Manager					&rm;
	MessageSharedPointerT	&rmsgptr;
	
	void operator()(Object &_robj){
		MessagePointerT msgptr(rmsgptr);
		if(_robj.notify(msgptr)){
			rm.raise(_robj);
		}
	}
};


}//namespace


bool Service::notifyAll(ulong _sm){
	if(isRegistered()){
		SignalNotifier	notifier(rm, _sm);
		return rm.forEachServiceObject(*this, notifier);
	}else{
		return false;
	}
}
bool Service::notifyAll(MessageSharedPointerT &_rmsgptr){
	if(isRegistered()){
		MessageNotifier notifier(rm, _rmsgptr);
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

ObjectUidT Service::unsafeRegisterObject(Object &_robj)const{
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


