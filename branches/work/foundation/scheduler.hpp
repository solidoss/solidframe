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

//! A template active container for objects needing complex handeling
/*!
	<b>Overview:</b><br>
	Complex handeling means that objects can reside within the
	pool for as much as they want, and are given processor time
	as result of different events.
	
	<b>Usage:</b><br>
	- Use the Scheduler together with a Selector, which will ensure
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
		typename S::JobT,
		ThisT&,
		Worker
	> 						WorkPoolT;
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
		uint16 _startthrcnt = 0,
		uint16 _maxthrcnt = 1,
		const IndexT &_selcap = 1024
	):SchedulerBase(_rm, _startthrcnt, _maxthrcnt, _selcap), wp(*this){
		//slotvec.reserve(_maxthcnt > 1024 ? 1024 : _maxthcnt);
	}
	//! Constructor
	/*!
		\param _startthrcnt The number of threads to create at start
		\param _maxthcnt The maximum count of threads that can be created
		\param _selcap The capacity of a selector - the total number
		of objects handled would be _maxthcnt * _selcap
	*/
	Scheduler(
		uint16 _startthrcnt = 0,
		uint16 _maxthrcnt = 1,
		const IndexT &_selcap = 1024
	):SchedulerBase(_startthrcnt, _maxthrcnt, _selcap),wp(*this){
		//slotvec.reserve(_maxthcnt > 1024 ? 1024 : _maxthcnt);
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
	static void schedule(const JobT &_rjb, uint _idx = 0){
		static_cast<ThisT*>(m().scheduler<ThisT>(_idx))->wp.push(_rjb);
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
		Worker	*pw(_rwp.createMultiWorker(0));
		if(pw && pw->start() != OK){
			delete pw;
			return false;
		}return true;
	}
	void prepareWorker(Worker &_rw){
		prepareThread(&_rw.s);
		_rw.s.prepare();
		_rw.s.reserve(selcap);
	}
	void unprepareWorker(Worker &_rw){
		unprepareThread(&_rw.s);
		_rw.s.unprepare();
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
			if(_rwp.size() && !tryRaiseOneSelector()){
				if(crtwkrcnt < maxwkrcnt){
					_rwp.createWorker();
				}else{
					return 64;//TODO:
				}
			}
			markSelectorFull(_rw.s);
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
			_rw.s.push(*it);
		}
		_rw.s.run();
	}
	void onStop(){
		this->doStop();
	}
private:
	WorkPoolT		wp;
};

}//namesspace foundation

#endif

