/* Declarations file scheduler.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FOUNDATION_SCHEDULER_HPP
#define FOUNDATION_SCHEDULER_HPP

#include <deque>

#include "utility/workpool.hpp"
#include "utility/list.hpp"

#include "foundation/manager.hpp"
#include "foundation/schedulerbase.hpp"


namespace foundation{

//! An active container for objects needing complex handeling
/*!
	<b>Overview:</b><br>
	Complex handeling means that objects can reside within the
	pool for as much as they want, and are given processor time
	as result of different events.
	
	<b>Usage:</b><br>
	- Use the SelectPool together with a selector, which will ensure
	object handeling at thread level.
	- Objects must implement "int execute(ulong _evs, TimeSpec &_rtout)" method.
*/
template <class S>
class Scheduler: public SchedulerBase, public WorkPoolControllerBase{
	typedef S				SelectorT;
	typedef Scheduler<S>	ThisT;
	struct Worker: WorkerBase{
		SelectorT	s;
	};
	typedef WorkPool<
		typename S::ObjectT,
		ThisT&,
		Worker
	> 						WorkPoolT;
	friend class WorkPool<
		typename S::ObjectT,
		ThisT&,
		Worker
	>;
public://definition
	typedef typename S::ObjectT		JobT;
	//! Constructor
	/*!
		\param _rm Reference to parent manager
		\param _maxthcnt The maximum count of threads that can be created
		\param _selcap The capacity of a selector - the total number
		of objects handled would be _maxthcnt * _selcap
	*/
	Scheduler(
		Manager &_rm,
		uint _maxthcnt = 1,
		const IndexT &_selcap = 1024
	):SchedulerBase(_rm), wp(*this), cap(0), selcap(_selcap){
		//slotvec.reserve(_maxthcnt > 1024 ? 1024 : _maxthcnt);
	}
	Scheduler(
		uint _maxthcnt = 1,
		const IndexT &_selcap = 1024
	):wp(*this), cap(0),selcap(_selcap){
		//slotvec.reserve(_maxthcnt > 1024 ? 1024 : _maxthcnt);
	}
	
	static void schedule(const JobT &_rjb, uint _idx = 0){
		static_cast<ThisT*>(m().scheduler<ThisT>(_idx))->wp.push(_rjb);
	}
	
	void start(ushort _minwkrcnt = 1, bool _wait = false){
		wp.start(_minwkrcnt, _wait);
	}
	
	void stop(bool _wait = true){
		wp.stop(_wait);
	}
private:
	typedef std::vector<JobT>	JobVectorT;
	void createWorker(WorkPoolT &_rwp){
		_rwp.createMultiWorker(0);
	}
	ulong onPopCheckWorkerAvailability(WorkPoolT &_rwp, Worker&_rw){
		return 1;
	}
	void onPopWakeOtherWorkers(WorkPoolT &_rwp){
	}
	void prepareWorker(Worker &_rw){
	}
	void unprepareWorker(Worker &_rw){
	}
	void onPush(WorkPoolT &_rwp){
	}
	void onMultiPush(WorkPoolT &_rwp, ulong _cnt){
	}
	ulong onPopStart(WorkPoolT &_rwp, Worker &_rw, ulong _maxcnt){
		return _maxcnt;
	}
	void onPopDone(WorkPoolT &_rwp){
	}
	void execute(JobVectorT &_rjobvec){
		
	}
private:
	WorkPoolT		wp;
	IndexT			cap;//the total count of objects already in pool
	IndexT			selcap;
	uint			thrid;
};

}//namesspace foundation

#endif

