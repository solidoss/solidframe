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
		ReactorT	reactor;
		
		static bool createAndStart(SchedulerBase &_rsched){
			Worker *pw = new Worker(_rsched);
			if(pw->start()){//create detached
				return true;
			}else{
				delete pw;
				return false;
			}
		}
		
		Worker(SchedulerBase &_rsched):reactor(_rsched){}
		
		void run(){
			reactor.run();
		}
	};
	
	struct ScheduleFct{
		ObjectPointerT &robjptr;
		
		ScheduleFct(ObjectPointerT &_robjptr):robjptr(_robjptr){}
		
		bool operator()(ReactorBase &_rreactor){
			return static_cast<ReactorT&>(_rreactor).push(robjptr);
		}
	};
public:
	
	Scheduler(Manager &_rm):SchedulerBase(_rm){}
	
	bool start(size_t _reactorcnt = 1, size_t _reactorchunkcp = 1024){
		return SchedulerBase::doStart(Worker::createAndStart, _reactorcnt, _reactorchunkcp);
	}

	void stop(bool _wait = true){
		SchedulerBase::doStop(_wait);
	}
	
	bool schedule(ObjectPointerT &_robjptr){
		ScheduleFct			s(_robjptr);
		ScheduleFunctorT	sfct(s);
		return doSchedule(*_robjptr, sfct);
	}
};

}//namespace frame
}//namespace solid

#endif

