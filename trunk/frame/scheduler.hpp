// frame/scheduler.hpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_SCHEDULER_HPP
#define SOLID_FRAME_SCHEDULER_HPP

#include <deque>

#include "utility/workpool.hpp"
#include "utility/list.hpp"

#include "frame/manager.hpp"
#include "frame/schedulerbase.hpp"

namespace solid{
namespace frame{

//! A template active container for objects
/*!
	<b>Overview:</b><br>
	Objects can reside within the Scheduler for as much as they want,
	and are given processor time as result of different events.
	The Scheduler will handle as much as _maxthrcnt * _selcap objects.
	The objects above this number, will wait in a queue.
	This is due to a aio::Selector limitation imposed by performance reasons
	(the deque is twice as slow as vector).
	
	<b>Usage:</b><br>
	- Use the Scheduler together with a Selector, which will ensure
	object handeling at thread level.<br>
	- Objects must implement "int execute(ulong _evs, TimeSpec &_rtout)" method.<br>
*/
template <class S>
class Scheduler: public SchedulerBase, public WorkPoolControllerBase{
	typedef S						SelectorT;
	typedef Scheduler<S>			ThisT;
	struct Worker: WorkerBase{
		SelectorT	s;
	};
	typedef WorkPool<
		typename S::JobT,
		ThisT&,
		Worker
	> 								WorkPoolT;
	friend class WorkPool<
		typename S::JobT,
		ThisT&,
		Worker
	>;
public://definition
	typedef typename S::JobT		JobT;
	typedef typename S::ObjectT		ObjectT;
	//! Constructor
	/*!
		\param _rm Reference to parent manager
		\param _startthrcnt The number of threads to create at start
		\param _maxthcnt The maximum count of threads that can be created
		\param _selcap The capacity of a selector - the total number
		of objects handled would be _maxthcnt * _selcap
	*/
	Scheduler(
		Manager &_rm,
		int16 _startthrcnt = 0,
		uint16 _maxthrcnt = 2,
		const IndexT &_selcap = 1024 * 64
	):SchedulerBase(_rm, _startthrcnt >= 0 ? _startthrcnt : -_startthrcnt, _maxthrcnt, _selcap), wp(*this){
		if(_startthrcnt >= 0){
			start(_startthrcnt);
		}
	}
	
	~Scheduler(){
	}
	
	
	//! Schedule a job 
	/*!
	 * \param _rjb The job structure
	 * \param _idx The index of the scheduler with this typedef
	 * <br><br>
	 * One can register multiple schedulers of the same type 
	 * using foundation::Manager::registerScheduler. The _idx
	 * parameter represents the registration number for schedulers with
	 * the same type.
	 */
	void schedule(const JobT &_rjb){
		wp.push(_rjb);
	}
	
	
	//! Starts the scheduler
	/*!
	 * Usually this is not called directly, but by the foundation::Manager
	 * start, through SchedulerBase::start<br>
	 * NOTE: in future versions this might be made protected or private
	 */
	void start(ushort _startwkrcnt = 0){
		wp.start(_startwkrcnt ? _startwkrcnt : startwkrcnt);
	}
	
	//! Starts the scheduler
	/*!
	 * Usually this is not called directly, but by the foundation::Manager
	 * stop, through SchedulerBase::stop
	 * * NOTE: in future versions this might be made protected or private
	 */
	void stop(bool _wait = true){
		wp.stop(_wait);
	}
	
private:
	
	typedef std::vector<JobT>	JobVectorT;
	bool createWorker(WorkPoolT &_rwp){
		++crtwkrcnt;
		Worker	*pw(_rwp.createMultiWorker(0));
		if(pw && !pw->start()){
			delete pw;
			--crtwkrcnt;
			return false;
		}return true;
	}
	bool prepareWorker(Worker &_rw){
		if(!prepareThread(&_rw.s)){
			return false;
		}
		_rw.s.prepare();
		return _rw.s.init(selcap);
	}
	void unprepareWorker(Worker &_rw){
		unprepareThread(&_rw.s);
		_rw.s.unprepare();
		--crtwkrcnt;
	}
	void onPush(WorkPoolT &_rwp){
		if(_rwp.size() != 1) return;
		if(tryRaiseOneSelector()) return;
		if(crtwkrcnt < maxwkrcnt){
			_rwp.createWorker();
		}else{
			raiseOneSelector();
		}
	}
	void onMultiPush(WorkPoolT &_rwp, ulong _cnt){
		//NOTE: not used right now
	}
	ulong onPopStart(WorkPoolT &_rwp, Worker &_rw, ulong){
		if(_rw.s.full()){
			markSelectorFull(_rw.s);
			if(_rwp.size() && !tryRaiseOneSelector()){
				if(crtwkrcnt < maxwkrcnt){
					_rwp.createWorker();
				}else{
					raiseOneSelector();
				}
			}
			return 0;
		}
		markSelectorNotFull(_rw.s);
		if(_rwp.empty() && !_rw.s.empty()) return 0;
		return _rw.s.capacity() - _rw.s.size();
	}
	void onPopDone(WorkPoolT &_rwp, Worker &_rw){
		if(_rwp.size()){
			if(_rw.s.full()){
				markSelectorFull(_rw.s);
				if(!tryRaiseOneSelector()){
					if(crtwkrcnt < maxwkrcnt){
						_rwp.createWorker();
					}else{
						raiseOneSelector();
					}
				}
			}else{
				bool b = tryRaiseOneSelector();
				cassert(b);
			}
		}
	}
	void execute(Worker &_rw, JobVectorT &_rjobvec){
		for(typename JobVectorT::iterator it(_rjobvec.begin()); it != _rjobvec.end(); ++it){
			if(!_rw.s.push(*it)){
				wp.push(*it);
			}
		}
		_rw.s.run();
	}
	void onStop(){
		this->doStop();
	}
private:
	WorkPoolT		wp;
};

}//namespace frame
}//namespace solid

#endif

