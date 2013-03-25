/* Implementation file service.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

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

#include "frame/service.hpp"
#include "frame/object.hpp"
#include "frame/manager.hpp"
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
		return rm.forEachServiceObject(*this, SignalNotifier(rm, _sm));
	}else{
		return false;
	}
}
bool Service::notifyAll(MessageSharedPointerT &_rmsgptr){
	if(isRegistered()){
		return rm.forEachServiceObject(*this, MessageNotifier(rm, _rmsgptr));
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
	const size_t	svcidx = idx.load(ATOMIC_NS::memory_order_relaxed);
	return rm.doRegisterServiceObject(svcidx, _robj);
}

void Service::unsafeStop(Locker<Mutex> &_rlock, bool _wait){
	const size_t	svcidx = idx.load(ATOMIC_NS::memory_order_relaxed);
	rm.doWaitStopService(svcidx, _rlock, true);
}
void Service::unsafeReset(Locker<Mutex> &_rlock){
	const size_t	svcidx = idx.load(ATOMIC_NS::memory_order_relaxed);
	rm.doResetService(svcidx, _rlock);
}

}//namespace frame
}//namespace solid


