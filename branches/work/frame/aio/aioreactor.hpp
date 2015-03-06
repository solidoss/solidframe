// frame/aio/reactor.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_REACTOR_HPP
#define SOLID_FRAME_AIO_REACTOR_HPP

#include "system/timespec.hpp"
#include "frame/common.hpp"
#include "frame/reactorbase.hpp"
#include "utility/dynamicpointer.hpp"
#include "frame/aio/aiocommon.hpp"

namespace solid{

struct TimeSpec;
struct Device;

namespace frame{

class Service;

namespace aio{

class Object;
struct ReactorContext;
struct CompletionHandler;

typedef DynamicPointer<Object>	ObjectPointerT;

//! 
/*!
	
*/
class Reactor: public frame::ReactorBase{
public:
	typedef ObjectPointerT		TaskT;
	typedef Object				ObjectT;
	
	Reactor(SchedulerBase &_rsched, const size_t _schedidx);
	~Reactor();
	
	void post(ReactorContext &_rctx, EventFunctionT  &_revfn, Event const &_rev);
	void post(ReactorContext &_rctx, EventFunctionT  &_revfn, Event const &_rev, CompletionHandler const &_rch);
	
	bool waitDevice(ReactorContext &_rctx, CompletionHandler const &_rch, Device const &_rsd, const ReactorWaitRequestsE _req);
	bool addDevice(ReactorContext &_rctx, Device const &_rsd, const ReactorWaitRequestsE _req);
	
	bool start();
	
	/*virtual*/ bool raise(UidT const& _robjuid, Event const& _revt);
	/*virtual*/ void stop();
	
	void registerCompletionHandler(CompletionHandler &_rch, Object const &_robj);
	void unregisterCompletionHandler(CompletionHandler &_rch);
	
	void run();
	bool push(TaskT &_robj, Service &_rsvc, Event const &_revt);
	
	Service& service(ReactorContext const &_rctx)const;
	
	Object& object(ReactorContext const &_rctx)const;
	UidT objectUid(ReactorContext const &_rctx)const;
	
	CompletionHandler *completionHandler(ReactorContext const &_rctx)const;
private:
	friend struct EventHandler;
	friend class CompletionHandler;
	
	static Reactor* safeSpecific();
	static Reactor& specific();
	
	void doCompleteIo(TimeSpec const &_rcrttime, const size_t _sz);
	void doCompleteTimer(TimeSpec  const &_rcrttime);
	void doCompleteExec(TimeSpec  const &_rcrttime);
	void doCompleteEvents(ReactorContext const &_rctx);
	void doStoreSpecific();
	void doClearSpecific();
	static void call_object_on_event(ReactorContext &_rctx, Event const &_rev);
private://data
	struct Data;
	Data	&d;
};


}//namespace aio
}//namespace frame
}//namespace solid

#endif

