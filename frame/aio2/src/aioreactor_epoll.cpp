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

#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "frame/object.hpp"
#include "frame/common.hpp"

#include "frame/aio2/aioreactor.hpp"
#include "frame/aio2/aioobject.hpp"


namespace solid{
namespace frame{
namespace aio{


typedef ATOMIC_NS::atomic<bool>		AtomicBoolT;
typedef ATOMIC_NS::atomic<size_t>	AtomicSizeT;
typedef Reactor::TaskT				TaskT;
struct NewTaskStub{
	NewTaskStub(
		UidT const&_ruid, TaskT const&_rtsk, Event const &_revt
	):uid(_ruid), tsk(_rtsk), evt(_revt){}
	
	NewTaskStub(){}
	UidT	uid;
	TaskT	tsk;
	Event	evt;
};

typedef std::vector<NewTaskStub>	NewTaskVectorT;
enum{
	EventCapacity = 4096
};

struct Reactor::Data{
	Data():epollfd(-1), running(0), crtpushtskvecidx(0), crtpushvecsz(0){}
	int					epollfd;
	Device				eventdev;
	AtomicBoolT			running;
	size_t				crtpushtskvecidx;
	AtomicSizeT			crtpushvecsz;
	
	Mutex				mtx;
	epoll_event 		events[EventCapacity];
	NewTaskVectorT		pushtskvec[2];
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
		edbgx(Debug::aio, "epoll_create: "<<strerror(errno));
		return false;
	}
	d.eventdev = Device(eventfd(0, EFD_NONBLOCK));
	if(!d.eventdev.ok()){
		edbgx(Debug::aio, "eventfd: "<<strerror(errno));
		return false;
	}
	epoll_event ev;
	ev.data.u64 = 0;
	ev.events = EPOLLIN | EPOLLPRI | EPOLLET;
	
	if(epoll_ctl(d.epollfd, EPOLL_CTL_ADD, d.eventdev.descriptor(), &ev)){
		edbgx(Debug::aio, "epoll_ctl: "<<strerror(errno));
		return false;
	}
	d.running = true;
	return true;
}

/*virtual*/ bool Reactor::raise(UidT const& _robjuid, Event const& _re){
	vdbgx(Debug::aio, "");
	return false;
}
/*virtual*/ void Reactor::stop(){
	vdbgx(Debug::aio, "");
	d.running = false;
	
	const uint64 v = 1;
	d.eventdev.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

//Called from outside reactor's thread
bool Reactor::push(TaskT &_rcon, Event const &_revt){
	vdbgx(Debug::aio, "");
	bool 	rv = true;
	size_t	pushvecsz = 0;
	{
		Locker<Mutex>	lock(d.mtx);
		const UidT		uid = this->popUid(*_rcon);

		d.pushtskvec[d.crtpushtskvecidx].push_back(NewTaskStub(uid, _rcon, _revt));
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
		selcnt = epoll_wait(d.epollfd, d.events, EventCapacity, -1);
		if(selcnt > 0){
			for(int i = 0; i < selcnt; ++i){
				if(d.events[i].data.u64 == 0){
					//event
					uint64 v = 0;
					d.eventdev.read(reinterpret_cast<char*>(&v), sizeof(v));
					vdbgx(Debug::aio, "Event - Stopping - read: "<<v);
					running = d.running;
				}else{
					
				}
			}
		}else if(selcnt < 0){
			running = false;
		}
	}
	vdbgx(Debug::aio, "<exit>");
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

