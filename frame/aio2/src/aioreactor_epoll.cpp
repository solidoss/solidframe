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
	
};

Reactor::Reactor(
	SchedulerBase &_rsched
):ReactorBase(_rsched), d(*(new Data)){}

Reactor::~Reactor(){
	delete &d;
}

/*virtual*/ bool Reactor::raise(UidT const& _robjuid, Event const& _re){
	return false;
}
/*virtual*/ void Reactor::stop(){
	
}
/*virtual*/ void Reactor::update(){
	
}

void Reactor::run(){
	
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

