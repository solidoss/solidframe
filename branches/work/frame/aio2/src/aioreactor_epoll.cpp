// frame/aio/src/aioselector_epoll.cpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "system/common.hpp"
#include <fcntl.h>
#include <sys/epoll.h>
#include "system/exception.hpp"

#include <sys/eventfd.h>

#include <vector>
#include <cerrno>
#include <cstring>

#include "system/debug.hpp"
#include "system/timespec.hpp"
#include "system/mutex.hpp"
#include "system/thread.hpp"
#include "system/device.hpp"
#include "system/error.hpp"

#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "frame/object.hpp"
#include "frame/service.hpp"
#include "frame/common.hpp"

#include "frame/aio2/aioreactor.hpp"
#include "frame/aio2/aioobject.hpp"
#include "frame/aio2/aiocompletion.hpp"


namespace solid{
namespace frame{
namespace aio{


typedef ATOMIC_NS::atomic<bool>		AtomicBoolT;
typedef ATOMIC_NS::atomic<size_t>	AtomicSizeT;
typedef Reactor::TaskT				TaskT;


struct NewTaskStub{
	NewTaskStub(
		UidT const&_ruid, TaskT const&_rtsk, Service &_rsvc, Event const &_revt
	):uid(_ruid), tsk(_rtsk), rsvc(_rsvc), evt(_revt){}
	
	//NewTaskStub(){}
	UidT		uid;
	TaskT		tsk;
	Service		&rsvc;
	Event		evt;
};

struct CompletionHandlerStub{
	CompletionHandlerStub(
		CompletionHandler *_pch = nullptr
	):pch(_pch), waitreq(ReactorWaitNone){}
	
	CompletionHandler		*pch;
	ReactorWaitRequestsE	waitreq;
};


enum{
	EventCapacity = 4096
};


typedef std::vector<NewTaskStub>			NewTaskVectorT;
typedef std::vector<epoll_event>			EpollEventVectorT;
typedef std::deque<CompletionHandlerStub>	CompletionHandlerDequeT;

struct Reactor::Data{
	Data():epollfd(-1), running(0), crtpushtskvecidx(0), crtpushvecsz(0){}
	
	int computeWaitTimeMilliseconds()const{
		//TODO:
		return -1;
	}
	
	int							epollfd;
	Device						eventdev;
	AtomicBoolT					running;
	size_t						crtpushtskvecidx;
	AtomicSizeT					crtpushvecsz;
	
	Mutex						mtx;
	epoll_event 				events[EventCapacity];
	EpollEventVectorT			eventvec;
	NewTaskVectorT				pushtskvec[2];
	CompletionHandlerDequeT		chdq;
};

Reactor::Reactor(
	SchedulerBase &_rsched,
	const size_t _idx 
):ReactorBase(_rsched, _idx), d(*(new Data)){
	vdbgx(Debug::aio, "");
}

Reactor::~Reactor(){
	delete &d;
	vdbgx(Debug::aio, "");
}

bool Reactor::start(){
	vdbgx(Debug::aio, "");
	d.epollfd = epoll_create(EventCapacity);
	if(d.epollfd < 0){
		edbgx(Debug::aio, "epoll_create: "<<last_system_error().message());
		return false;
	}
	d.eventdev = Device(eventfd(0, EFD_NONBLOCK));
	if(!d.eventdev.ok()){
		edbgx(Debug::aio, "eventfd: "<<last_system_error().message());
		return false;
	}
	epoll_event ev;
	ev.data.u64 = 0;
	ev.events = EPOLLIN | EPOLLPRI | EPOLLET;
	
	if(epoll_ctl(d.epollfd, EPOLL_CTL_ADD, d.eventdev.descriptor(), &ev)){
		edbgx(Debug::aio, "epoll_ctl: "<<last_system_error().message());
		return false;
	}
	d.eventvec.resize(EventCapacity);
	d.running = true;
	return true;
}

/*virtual*/ bool Reactor::raise(UidT const& _robjuid, Event const& _revt){
	vdbgx(Debug::aio,  (void*)this<<" uid = "<<_robjuid.index<<','<<_robjuid.unique<<" event id = "<<_revt.id);
	return false;
}
/*virtual*/ void Reactor::stop(){
	vdbgx(Debug::aio, "");
	d.running = false;
	
	const uint64 v = 1;
	d.eventdev.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

//Called from outside reactor's thread
bool Reactor::push(TaskT &_robj, Service &_rsvc, Event const &_revt){
	vdbgx(Debug::aio, (void*)this);
	bool 	rv = true;
	size_t	pushvecsz = 0;
	{
		Locker<Mutex>	lock(d.mtx);
		const UidT		uid = this->popUid(*_robj);
		
		vdbgx(Debug::aio, (void*)this<<" uid = "<<uid.index<<','<<uid.unique<<" event id = "<<_revt.id);
			
		d.pushtskvec[d.crtpushtskvecidx].push_back(NewTaskStub(uid, _robj, _rsvc, _revt));
		pushvecsz = d.pushtskvec[d.crtpushtskvecidx].size();
		
	}
	d.crtpushvecsz = pushvecsz;
	if(pushvecsz == 1){
		const uint64 v = 1;
		d.eventdev.write(reinterpret_cast<const char*>(&v), sizeof(v));
	}
	return rv;
}


void Reactor::run(){
	vdbgx(Debug::aio, "<enter>");
	int		selcnt;
	bool	running = true;
	while(running){
		selcnt = epoll_wait(d.epollfd, d.eventvec.data(), d.eventvec.size(), d.computeWaitTimeMilliseconds());
		
		if(selcnt > 0){
			doCompleteIo(selcnt);
		}else if(selcnt < 0 && errno != EINTR){
			edbgx(Debug::aio, "epoll_wait errno  = "<<last_system_error().message());
			running = false;	
		}
		
		doCompleteTimer();
		
		doCompleteExec();
		
	}
	vdbgx(Debug::aio, "<exit>");
}

void Reactor::doCompleteIo(const size_t _sz){
	for(int i = 0; i < _sz; ++i){
		epoll_event				&rev = d.eventvec[i];
		CompletionHandlerStub	&rch = d.chdq[rev.data.u64];
		
		if(rch.pch){
			rch.pch->handleCompletion();
		}
		
// 		if(d.eventvec[i].data.u64 == 0){
// 			//event
// 			uint64 v = 0;
// 			d.eventdev.read(reinterpret_cast<char*>(&v), sizeof(v));
// 			vdbgx(Debug::aio, "Event - Stopping - read: "<<v);
// 			//running = d.running;
// 		}
	}
}
void Reactor::doCompleteTimer(){
	
}

void Reactor::doCompleteExec(){
	
}

void Reactor::wait(ReactorContext &_rctx, CompletionHandler const *_pch, const ReactorWaitRequestsE _req){
	vdbgx(Debug::aio, "");
}

void Reactor::registerCompletionHandler(CompletionHandler &_rch){
	vdbgx(Debug::aio, "");
}

void Reactor::unregisterCompletionHandler(CompletionHandler &_rch){
	vdbgx(Debug::aio, "");
}

/*static*/ Reactor* Reactor::safeSpecific(){
	vdbgx(Debug::aio, "");
	return NULL;
}

/*static*/ Reactor& Reactor::specific(){
	vdbgx(Debug::aio, "");
	return *safeSpecific();
}

}//namespace aio
}//namespace frame
}//namespace solid

