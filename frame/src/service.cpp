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
	Manager &_rm,
	Event const &_rstopevt
):rm(_rm), stopevent(_rstopevt), idx(-1){
	_rm.registerService(*this);
}

Service::~Service(){
	stop(true);
	rm.unregisterService(*this);
}

void Service::notifyAll(Event const & _revt, const size_t _sigmsk/* = 0*/){
	EventNotifierF	notifier(rm, _revt, _sigmsk);
	rm.forEachServiceObject(*this, notifier);
}

void Service::stop(const bool _wait){
	rm.stopService(*this, _wait);
}

Mutex& Service::mutex()const{
	return rm.mutex(*this);
}


ObjectUidT Service::registerObject(ObjectBase &_robj, ReactorBase &_rr, ScheduleFunctorT &_rfct, ErrorConditionT &_rerr){
	return rm.registerObject(*this, _robj, _rr, _rfct, _rerr);
}

// void Service::unsafeStop(Locker<Mutex> &_rlock, bool _wait){
// 	const size_t	svcidx = idx.load(/*ATOMIC_NS::memory_order_seq_cst*/);
// 	rm.doWaitStopService(svcidx, _rlock, true);
// }

}//namespace frame
}//namespace solid


