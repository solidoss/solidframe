// solid/frame/scheduler.hpp
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

#include "solid/utility/dynamicpointer.hpp"
#include "solid/frame/manager.hpp"
#include "solid/frame/schedulerbase.hpp"

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
	
	struct Worker{
		static void run(SchedulerBase *_psched, const size_t _idx){
			ReactorT	reactor(*_psched, _idx);
			
			if(!reactor.prepareThread(reactor.start())){
				return;
			}
			reactor.run();
			reactor.unprepareThread();
		}
		
		static bool create(SchedulerBase &_rsched, const size_t _idx, std::thread &_rthr){
			bool rv = false;
			try{
				_rthr = std::thread(Worker::run, &_rsched, _idx);
				rv = true;
			}catch(...){
				
			}
			return rv;
		}
		
	};
	
	struct ScheduleCommand{
		ObjectPointerT	&robjptr;
		Service			&rsvc;
		Event 			&&revt;
		
		ScheduleCommand(ObjectPointerT &_robjptr, Service &_rsvc, Event &&_revt):robjptr(_robjptr), rsvc(_rsvc), revt(std::move(_revt)){}
		
		bool operator()(ReactorBase &_rreactor){
			return static_cast<ReactorT&>(_rreactor).push(robjptr, rsvc, std::move(revt));
		}
	};
public:
	
	Scheduler(){}
	
	ErrorConditionT start(const size_t _reactorcnt = 1){
		ThreadEnterFunctionT		enf;
		ThreadExitFunctionT 		exf;
		return SchedulerBase::doStart(Worker::create, enf, exf, _reactorcnt);
	}
	
	template <class EnterFct, class ExitFct>
	ErrorConditionT start(EnterFct _enf, ExitFct _exf, const size_t _reactorcnt = 1){
		ThreadEnterFunctionT		enf(_enf);
		ThreadExitFunctionT 		exf(_exf);//we don't want to copy _exf
		return SchedulerBase::doStart(Worker::create, enf, exf, _reactorcnt);
	}

	void stop(const bool _wait = true){
		SchedulerBase::doStop(_wait);
	}
	
	
	ObjectIdT	startObject(
		ObjectPointerT &_robjptr, Service &_rsvc,
		Event &&_revt, ErrorConditionT &_rerr
	){
		ScheduleCommand		cmd(_robjptr, _rsvc, std::move(_revt));
		ScheduleFunctionT	fct([&cmd](ReactorBase &_rreactor){return cmd(_rreactor);});
		
		return doStartObject(*_robjptr, _rsvc, fct, _rerr);
	}
};

}//namespace frame
}//namespace solid

#endif

