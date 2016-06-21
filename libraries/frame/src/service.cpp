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
#include <condition_variable>
#include "system/exception.hpp"

#include "system/mutualstore.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "frame/manager.hpp"
#include "frame/service.hpp"
#include "frame/object.hpp"
#include "frame/common.hpp"

namespace solid{
namespace frame{

Service::Service(
	Manager &_rm
):rm(_rm), idx(static_cast<size_t>(InvalidIndex())), running(false){
	_rm.registerService(*this);
	vdbgx(Debug::frame, ""<<this);
}

Service::~Service(){
	vdbgx(Debug::frame, ""<<this);
	stop(true);
	rm.unregisterService(*this);
	vdbgx(Debug::frame, ""<<this);
}

void Service::notifyAll(Event const & _revt, const size_t _sigmsk/* = 0*/){
	rm.notifyAll(*this, _revt, _sigmsk);
}


bool Service::start(){
	return rm.startService(*this);
}

void Service::stop(const bool _wait){
	rm.stopService(*this, _wait);
}

std::mutex& Service::mutex(const ObjectBase &_robj)const{
	return rm.mutex(_robj);
}

std::mutex& Service::mutex()const{
	return rm.mutex(*this);
}


ObjectIdT Service::registerObject(ObjectBase &_robj, ReactorBase &_rr, ScheduleFunctionT &_rfct, ErrorConditionT &_rerr){
	return rm.registerObject(*this, _robj, _rr, _rfct, _rerr);
}

// void Service::unsafeStop(Locker<Mutex> &_rlock, bool _wait){
// 	const size_t	svcidx = idx.load(/*std::memory_order_seq_cst*/);
// 	rm.doWaitStopService(svcidx, _rlock, true);
// }

}//namespace frame
}//namespace solid


