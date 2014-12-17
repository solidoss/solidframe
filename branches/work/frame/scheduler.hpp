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
			if(!reactor.prepareThread()){
				return;
			}
			reactor.run();
			reactor.unprepareThread();
		}
	};
	
	struct ScheduleCommand{
		ObjectPointerT &robjptr;
		
		ScheduleCommand(ObjectPointerT &_robjptr):robjptr(_robjptr){}
		
		bool operator()(ReactorBase &_rreactor){
			return static_cast<ReactorT&>(_rreactor).push(robjptr);
		}
	};
public:
	
	Scheduler(Manager &_rm):SchedulerBase(_rm){}
	
	ErrorConditionT start(size_t _reactorcnt = 1){
		return SchedulerBase::doStart(Worker::create, _reactorcnt);
	}

	void stop(bool _wait = true){
		SchedulerBase::doStop(_wait);
	}
	
	ErrorConditionT schedule(ObjectPointerT &_robjptr){
		ScheduleCommand		cmd(_robjptr);
		ScheduleFunctorT	sfct(cmd);
		return doSchedule(*_robjptr, sfct);
	}
};

}//namespace frame
}//namespace solid

#endif

