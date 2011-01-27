/* Declarations file workpool.hpp
	
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

#ifndef UTILITY_WORKPOOL_HPP
#define UTILITY_WORKPOOL_HPP

#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"

#include "utility/common.hpp"
#include "utility/queue.hpp"

//! Base class for every workpool workers
struct WorkerBase: Thread{
	uint32	wkrid;
};

//! Base class for workpool
struct WorkPoolBase{
	enum State{
		Stopped = 0,
		Stopping,
		Running
	};
	State state()const{
		return st;
	}
	void state(State _s){
		st = _s;
	}
	bool isRunning()const{
		return st == Running;
	}
protected:
	WorkPoolBase():st(Stopped), wkrcnt(0) {}
	State						st;
	int							wkrcnt;
	Condition					thrcnd;
	Condition					sigcnd;
	Mutex						mtx;
};

//! A controller structure for WorkPool
/*!
 * The WorkPool will call methods of this structure.
 * Inherit and implement the needed methods.
 * No need for virtualization as the Controller is
 * a template parameter for WorkPool.
 */
struct WorkPoolControllerBase{
	void prepareWorker(WorkerBase &_rw){
	}
	void unprepareWorker(WorkerBase &_rw){
	}
	void onPush(WorkPoolBase &){
	}
	void onMultiPush(WorkPoolBase &, ulong _cnd){
	}
	ulong onPopStart(WorkPoolBase &, WorkerBase &, ulong _maxcnt){
		return _maxcnt;
	}
	void onPopDone(WorkPoolBase &, WorkerBase &){
	}
	void onStop(){
	}
};

//! A template workpool
/*!
 * The template parameters are:<br>
 * J - the Job type to be processed by the workpool. I
 * will hold a Queue\<J\>.<br>
 * C - WorkPool controller, the workpool will call controller
 * methods on different ocasions / events<br>
 * W - Base class for workers. Specify this if you want certain data
 * kept per worker. The workpool's actual workers will publicly
 * inherit W.<br>
 */
template <class J, class C, class W = WorkerBase>
class WorkPool: public WorkPoolBase{
	typedef std::vector<J>		JobVectorT;
	typedef WorkPool<J, C, W>	ThisT;
	struct SingleWorker: W{
		SingleWorker(ThisT &_rw):rw(_rw){}
		void run(){
			J	job;
			rw.enterWorker(*this);
			while(rw.pop(*this, job)){
				rw.execute(*this, job);
			}
			rw.exitWorker(*this);
		}
		ThisT	&rw;
	};
	struct MultiWorker: W{
		MultiWorker(ThisT &_rw, ulong _maxcnt):rw(_rw), maxcnt(_maxcnt){}
		void run(){
			JobVectorT	jobvec;
			if(maxcnt == 0) maxcnt = 1;
			rw.enterWorker(*this);
			while(rw.pop(*this, jobvec, maxcnt)){
				rw.execute(*this, jobvec);
				jobvec.clear();
			}
			rw.exitWorker(*this);
		}
		ThisT	&rw;
		ulong	maxcnt;
	};
	
public:
	typedef ThisT	WorkPoolT;
	typedef C		ControllerT;
	typedef W		WorkerT;
	typedef J		JobT;
	
	
	WorkPool(){
	}
	
	template <class T>
	WorkPool(T &_rt):ctrl(_rt){
	}
	
	~WorkPool(){
		
	}
	
	//! Push a new job
	void push(const JobT& _jb){
		mtx.lock();
		jobq.push(_jb);
		sigcnd.signal();
		ctrl.onPush(*this);
		mtx.unlock();
	}
	//! Push a table of jobs of size _cnt
	template <class I>
	void push(I _i, const I &_end){
		mtx.lock();
		ulong cnt(_end - _i);
		for(; _i != _end; ++_i){
			jobq.push(*_i);
		}
		sigcnd.broadcast();
		ctrl.onMultiPush(*this, cnt);
		mtx.unlock();
	}
	//! Starts the workpool, creating _minwkrcnt
	void start(ushort _minwkrcnt = 0){
		Mutex::Locker lock(mtx);
		if(state() == Running){
			return;
		}
		if(state() != Stopped){
			doStop(true);
		}
		wkrcnt = 0;
		state(Running);
		for(ushort i(0); i < _minwkrcnt; ++i){
			createWorker();
		}
	}
	//! Initiate workpool stop
	/*!
		It can block waiting for all workers to stop or just initiate stopping.
		The ideea is that when one have M workpools, it is faster to
		first initiate stop for all pools (wp[i].stop(false)) and then wait
		for actual stoping (wp[i].stop(true))
	*/
	void stop(bool _wait = true){
		Mutex::Locker	lock(mtx);
		doStop(_wait);
	}
	ulong size()const{
		return jobq.size();
	}
	bool empty()const{
		return jobq.empty();
	}
	void createWorker(){
		cassert(!mtx.tryLock());
		++wkrcnt;
		if(ctrl.createWorker(*this)){
		}else{
			--wkrcnt;
			thrcnd.broadcast();
		}
	}
	WorkerT* createSingleWorker(){
		return new SingleWorker(*this);
	}
	WorkerT* createMultiWorker(ulong _maxcnt){
		return new MultiWorker(*this, _maxcnt);
	}
private:
	friend struct SingleWorker;
	friend struct MultiWorker;
	void doStop(bool _wait){
		if(state() == Stopped) return;
		state(Stopping);
		sigcnd.broadcast();
		ctrl.onStop();
		if(!_wait) return;
		while(wkrcnt){
			thrcnd.wait(mtx);
		}
		state(Stopped);
	}
	bool pop(WorkerT &_rw, JobVectorT &_rjobvec, ulong _maxcnt){
		Mutex::Locker lock(mtx);
		uint32 insertcount(ctrl.onPopStart(*this, _rw, _maxcnt));
		if(!insertcount){
			return true;
		}
		if(doWaitJob()){
			do{
				_rjobvec.push_back(jobq.front());
				jobq.pop();
			}while(jobq.size() && --insertcount);
			ctrl.onPopDone(*this, _rw);
			return true;
		}
		return false;
	}
	
	bool pop(WorkerT &_rw, JobT &_rjob){
		Mutex::Locker lock(mtx);
		if(ctrl.onPopStart(*this, _rw, 1) == 0){
			sigcnd.signal();
			return false;
		}
		if(doWaitJob()){
			_rjob = jobq.front();
			jobq.pop();
			ctrl.onPopDone(*this, _rw);
			return true;
		}
		return false;
	}
	
	ulong doWaitJob(){
		while(jobq.empty() && isRunning()){
			sigcnd.wait(mtx);
		}
		return jobq.size();
	}
	
	void enterWorker(WorkerT &_rw){
		mtx.lock();
		ctrl.prepareWorker(_rw);
		//++wkrcnt;
		thrcnd.broadcast();
		mtx.unlock();
	}
	void exitWorker(WorkerT &_rw){
		mtx.lock();
		ctrl.unprepareWorker(_rw);
		--wkrcnt;
		cassert(wkrcnt >= 0);
		thrcnd.broadcast();
		mtx.unlock();
	}
	void execute(WorkerT &_rw, JobT &_rjob){
		ctrl.execute(_rw, _rjob);
	}
	void execute(WorkerT &_rw, JobVectorT &_rjobvec){
		ctrl.execute(_rw, _rjobvec);
	}
private:
	Queue<JobT>					jobq;
	ControllerT					ctrl;
	
};


#endif



