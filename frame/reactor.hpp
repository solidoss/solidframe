// frame/reactor.hpp
//
// Copyright (c) 2007, 2008, 2013,2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_REACTOR_HPP
#define SOLID_FRAME_REACTOR_HPP

#include "utility/dynamicpointer.hpp"
#include "frame/common.hpp"
#include "frame/reactorbase.hpp"
#include "frame/reactorcontext.hpp"


namespace solid{
class TimeSpec;
namespace frame{

class Service;
class Object;
struct ReactorContext;
struct CompletionHandler;
struct ChangeTimerIndexCallback;
struct TimerCallback;

typedef DynamicPointer<Object>	ObjectPointerT;

//! 
/*!
	
*/
class Reactor: public frame::ReactorBase{
	typedef FUNCTION<void(ReactorContext&, Event const &)>		EventFunctionT;
	template <class Function>
	struct StopObjectF{
		Function	function;
		bool		repost;
		
		explicit StopObjectF(Function &_function):function(std::move(_function)), repost(true){}
		
		//explicit StopObjectF(StopObjectF<Function> &&_rfnc):function(std::move(_rfnc.function)), repost(_rfnc.repost){}
		
		void operator()(ReactorContext& _rctx, Event const &_revent){
			if(repost){//skip one round - to guarantee that all remaining posts were delivered
				repost = false;
				EventFunctionT	eventfnc(*this);
				_rctx.reactor().doPost(_rctx, eventfnc, _revent);
			}else{
				function(_rctx, _revent);
				_rctx.reactor().doStopObject(_rctx);
			}
		}
	};
public:
	typedef ObjectPointerT		TaskT;
	typedef Object				ObjectT;
	
	Reactor(SchedulerBase &_rsched, const size_t _schedidx);
	~Reactor();
	
	template <typename Function>
	void post(ReactorContext &_rctx, Function _fnc, Event const &_rev){
		EventFunctionT	eventfnc(_fnc);
		doPost(_rctx, eventfnc, _rev);
	}
	
	template <typename Function>
	void post(ReactorContext &_rctx, Function _fnc, Event const &_rev, CompletionHandler const &_rch){
		EventFunctionT	eventfnc(_fnc);
		doPost(_rctx, eventfnc, _rev, _rch);
	}
	
	void postObjectStop(ReactorContext &_rctx);
	
	template <typename Function>
	void postObjectStop(ReactorContext &_rctx, Function _f, Event const &_rev){
		StopObjectF<Function>	stopfnc(_f);
		EventFunctionT			eventfnc(stopfnc);
		doPost(_rctx, eventfnc, _rev);
	}
	
	bool addTimer(CompletionHandler const &_rch, TimeSpec const &_rt, size_t &_rstoreidx);
	bool remTimer(CompletionHandler const &_rch, size_t const &_rstoreidx);
	
	bool start();
	
	/*virtual*/ bool raise(UniqueId const& _robjuid, Event const& _revt);
	/*virtual*/ void stop();
	
	void registerCompletionHandler(CompletionHandler &_rch, Object const &_robj);
	void unregisterCompletionHandler(CompletionHandler &_rch);
	
	void run();
	bool push(TaskT &_robj, Service &_rsvc, Event const &_revt);
	
	Service& service(ReactorContext const &_rctx)const;
	
	Object& object(ReactorContext const &_rctx)const;
	UniqueId objectUid(ReactorContext const &_rctx)const;
	
	CompletionHandler *completionHandler(ReactorContext const &_rctx)const;
private:
	friend struct EventHandler;
	friend class CompletionHandler;
	friend struct ChangeTimerIndexCallback;
	friend struct TimerCallback;
	friend struct ExecStub;
	
	static Reactor* safeSpecific();
	static Reactor& specific();
	
	bool doWaitEvent(TimeSpec const &_rcrttime);
	
	void doCompleteTimer(TimeSpec  const &_rcrttime);
	void doCompleteExec(TimeSpec  const &_rcrttime);
	void doCompleteEvents(TimeSpec const &_rcrttime);
	void doStoreSpecific();
	void doClearSpecific();
	void doUpdateTimerIndex(const size_t _chidx, const size_t _newidx, const size_t _oldidx);
	
	void doPost(ReactorContext &_rctx, EventFunctionT  &_revfn, Event const &_rev);
	void doPost(ReactorContext &_rctx, EventFunctionT  &_revfn, Event const &_rev, CompletionHandler const &_rch);
	
	void doStopObject(ReactorContext &_rctx);
	
	void onTimer(ReactorContext &_rctx, const size_t _tidx, const size_t _chidx);
	static void call_object_on_event(ReactorContext &_rctx, Event const &_rev);
	static void stop_object_repost(ReactorContext &_rctx, Event const &_revent);
	static void stop_object(ReactorContext &_rctx, Event const &_revent);
private://data
	struct Data;
	Data	&d;
};

}//namespace frame
}//namespace solid

#endif
