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

#ifndef UPIPESIGNAL
	#ifdef HAS_EVENTFD_H
		#include <sys/eventfd.h>
	#else 
		#define UPIPESIGNAL
	#endif
#endif

#include <vector>
#include <cerrno>
#include <cstring>

#include "system/debug.hpp"
#include "system/timespec.hpp"
#include "system/mutex.hpp"
#include "system/thread.hpp"

#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "frame/object.hpp"
#include "frame/common.hpp"

#include "frame/aio2/aioreactor.hpp"
#include "frame/aio2/aioobject.hpp"


namespace solid{
namespace frame{
namespace aio{

struct Reactor::Data{
	int					epollfd;
	int					eventfd;
	bool				running;
	epoll_event 		events[4096];
};

Reactor::Reactor(
	SchedulerBase &_rsched,
	const size_t _idx 
):ReactorBase(_rsched, _idx), d(*(new Data)){}

Reactor::~Reactor(){
	delete &d;
}

bool Reactor::start(){
	d.epollfd = epoll_create(4096);
	if(d.epollfd < 0){
		edbgx(Debug::aio, "epoll_create: "<<strerror(errno));
		return false;
	}
	d.eventfd = eventfd(0, EFD_NONBLOCK);
	if(d.eventfd < 0){
		edbgx(Debug::aio, "eventfd: "<<strerror(errno));
		return false;
	}
	epoll_event ev;
	ev.data.u64 = 0;
	ev.events = EPOLLIN | EPOLLPRI;
	
	if(epoll_ctl(d.epollfd, EPOLL_CTL_ADD, d.eventfd, &ev)){
		edbgx(Debug::aio, "epoll_ctl: "<<strerror(errno));
		return false;
	}
	d.running = true;
	return true;
}

/*virtual*/ bool Reactor::raise(UidT const& _robjuid, Event const& _re){
	return false;
}
/*virtual*/ void Reactor::stop(){
	uint8 v(1);
	d.running = true;
	::write(d.eventfd, &v, sizeof(v));
}
/*virtual*/ void Reactor::update(){
	
}

void Reactor::run(){
	int		selcnt;
	while(d.running){
		selcnt = epoll_wait(d.epollfd, d.events, Data::MAX_EVENTS_COUNT, pollwait);
	}
}

bool Reactor::push(JobT &_rcon){
	return false;
}

void Reactor::wait(ReactorContext &_rctx, CompletionHandler *_pch, ReactorWaitRequestsE _req){
	
}

void Reactor::registerCompletionHandler(CompletionHandler &_rch){
	
}

void Reactor::unregisterCompletionHandler(CompletionHandler &_rch){
	
}

/*static*/ Reactor* Reactor::safeSpecific(){
	return NULL;
}

/*static*/ Reactor& Reactor::specific(){
	return *safeSpecific();
}

}//namespace aio
}//namespace frame
}//namespace solid

