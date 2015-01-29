// frame/scheduler.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_SCHEDULER_HPP
#define SOLID_FRAME_SCHEDULER_HPP

#include "system/thread.hpp"
#include "utility/dynamicpointer.hpp"
#include "frame/manager.hpp"
#include "frame/schedulerbase.hpp"

namespace solid{
namespace frame{

class Service;

template <class R>
class Scheduler: private SchedulerBase{
public:
	typedef typename R::ObjectT			ObjectT;
	typedef DynamicPointer<ObjectT>		ObjectPointerT;
private:
	typedef R		ReactorT;
	struct Worker: Thread{
		SchedulerBase	&rsched;
		const size_t	idx;
		
		static Thread* create(SchedulerBase &_rsched, const size_t _idx){
			return new Worker(_rsched, _idx);
		}
		
		Worker(SchedulerBase &_rsched, const size_t _idx):rsched(_rsched), idx(_idx){}
		
		void run(){
			ReactorT	reactor(rsched, idx);
			if(!reactor.prepareThread(reactor.start())){
				return;
			}
			reactor.run();
			reactor.unprepareThread();
		}
	};
	
	struct ScheduleCommand{
		ObjectPointerT	&robjptr;
		Service			&rsvc;
		Event const 	&revt;
		
		ScheduleCommand(ObjectPointerT &_robjptr, Service &_rsvc, Event const &_revt):robjptr(_robjptr), rsvc(_rsvc), revt(_revt){}
		
		bool operator()(ReactorBase &_rreactor){
			return static_cast<ReactorT&>(_rreactor).push(robjptr, rsvc, revt);
		}
	};
public:
	
	Scheduler(){}
	
	ErrorConditionT start(const size_t _reactorcnt = 1){
		ThreadEnterFunctorT		enf;
		ThreadExitFunctorT 		exf;
		return SchedulerBase::doStart(Worker::create, enf, exf, _reactorcnt);
	}
	
	template <class EnterFct, class ExitFct>
	ErrorConditionT start(EnterFct _enf, ExitFct _exf, const size_t _reactorcnt = 1){
		ThreadEnterFunctorT		enf(_enf);
		ThreadExitFunctorT 		exf(_exf);
		return SchedulerBase::doStart(Worker::create, enf, exf, _reactorcnt);
	}

	void stop(const bool _wait = true){
		SchedulerBase::doStop(_wait);
	}
	
	
	ObjectUidT	startObject(
		ObjectPointerT &_robjptr, Service &_rsvc,
		Event const &_revt, ErrorConditionT &_rerr
	){
		ScheduleCommand		cmd(_robjptr, _rsvc, _revt);
		ScheduleFunctorT	fct(cmd);
		
		return doStartObject(*_robjptr, _rsvc, fct, _rerr);
	}
};

}//namespace frame
}//namespace solid

#endif

