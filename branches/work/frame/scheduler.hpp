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

template <class S>
class Scheduler: private SchedulerBase{
public:
	typedef typename S::ObjectT				ObjectT;
	typedef DynamicPointer<ObjectT>	ObjectPointerT;
private:
	typedef S		SelectorT;
	struct Worker: Thread{
		SelectorT	sel;
		
		static bool createAndStart(SchedulerBase &_rsched){
			Worker *pw = new Worker(_rsched);
			if(pw->start()){//create detached
				return true;
			}else{
				delete pw;
				return false;
			}
		}
		
		Worker(SchedulerBase &_rsched):sel(_rsched){}
		
		void run(){
			sel.run();
		}
	};
	
	struct ScheduleFct{
		ObjectPointerT &robjptr;
		
		ScheduleFct(ObjectPointerT &_robjptr):robjptr(_robjptr){}
		
		bool operator()(SelectorBase &_rsel){
			return static_cast<SelectorT&>(_rsel).push(robjptr);
		}
	};
public:
	
	Scheduler(Manager &_rm):SchedulerBase(_rm){}
	
	bool start(size_t _selcnt = 1, size_t _selchunkcp = 1024){
		return SchedulerBase::doStart(Worker::createAndStart, _selcnt, _selchunkcp);
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

