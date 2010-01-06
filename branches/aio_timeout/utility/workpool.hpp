/* Declarations file workpool.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef WORKPOOL_HPP
#define WORKPOOL_HPP

#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"

#include "common.hpp"
#include "queue.hpp"
//! A simple worker initialization plugin
struct WorkPoolPlugin{
	//! Virtual destructor
	virtual ~WorkPoolPlugin();
	//! This is called by every worker on Thread::prepare
	virtual void prepare();
	//! This is called by every worker on Thread::unprepare
	virtual void unprepare();
	//! It is called on workpool destructor
	/*!
		It is designed to allow using the same statically created
		plugin for many workpools.
		If release returns true, the plugin is deleted else not.
	*/
	virtual bool release()const;
};

WorkPoolPlugin* basicWorkPoolPlugin();
//! A template workpool/thread pool for processing jobs.
/*!
	I've tried to design it as flexible as possible. Basically all you need is to
	inherit the workpool and:
	- implement the not necessarily virtual run(Worker) method and use pop methods
		either in its single job pop form or the multy job form.
	- implement the createWorker(uint) which will create the requested number of workers
	- you can define your own workers inheriting the WorkPool::Worker but you should 
	use WorkPool::GenericWorker\<YourWorker\> to use the implemented prepare and unprepare
	methods. For that there is a commodity template method: createWorker.
	
	The interface is quite straight forward: one can push jobs, start and stop
	the pool.
	
	\see foundation::ExecPool and/or foundation::SelectPool.
*/
template <class Jb>
class WorkPool{
public:
	typedef WorkPool<Jb>	WorkPoolTp;
	//! Push a new job
	void push(const Jb& _jb){
		mtx.lock();
		cassert(state != Stopped);
		q.push(_jb);
		sigcnd.signal();
		mtx.unlock();
	}
	//! Push a table of jobs of size _cnt
	void push(const Jb *_pjb, unsigned _cnt){
		if(!_cnt) return;
		mtx.lock();
		cassert(state != Stopped);
		while(_cnt--){
			q.push(*_pjb);
			++_pjb;
		}
		sigcnd.signal();
		mtx.unlock();
	}
	//! Tries to push a job
	/*! Will return BAD if the pool is stoped
		else will return OK
	*/
	int tryPush(const Jb &_jb){
		if(state < Running) return BAD;
		push(_jb);
		return OK;
	}
	//! Tries to push a table of jobs of size _cnt
	/*! Will return BAD if the pool is stoped
		else will return OK
	*/
	int tryPush(const Jb *_pjb, int _cnt){
		if(state < Running) return BAD;
		push(_pjb,_cnt);
		return OK;
	}
	//! Starts the workpool, creating _minwkrcnt
	virtual int start(ushort _minwkrcnt, bool _wait = false){
		Mutex::Locker lock(mtx);
		if(state > Stopped) return BAD;
		_minwkrcnt = (_minwkrcnt)?_minwkrcnt:1;
		wkrcnt = 0;
		state = Running;
		idbgx(Dbg::utility, "before create workers");
		createWorkers(_minwkrcnt);
		idbgx(Dbg::utility, "after create");
		if(_wait){
			while(wkrcnt != _minwkrcnt){
				idbgx(Dbg::utility, "minwkrcnt = "<<_minwkrcnt<<" wkrcnt = "<<wkrcnt);
				thrcnd.wait(mtx);
			}
		}
		idbgx(Dbg::utility, "done start");
		return OK;
	}
	//! Initiate workpool stop
	/*!
		It can block waiting for all workers to stop or just initiate stopping.
		The ideea is that when one have M workpools, it is faster to
		first initiate stop for all pools (wp[i].stop(false)) and then wait
		for actual stoping (wp[i].stop(true))
	*/
	virtual void stop(bool _wait = true){
		Mutex::Locker	lock(mtx);
		if(state == Stopped) return;
		state = Stopping;
		sigcnd.broadcast();
		if(!_wait) return;
		while(wkrcnt)	thrcnd.wait(mtx);
		idbgx(Dbg::utility, "workpool::stopped"<<std::flush);
		state = Stopped;
	}
protected:
	//! Pop a job - called from WorkPool::run
	/*!
		This must be called in a while loop by the concrete workpool's run method.
		If no job is available, it will block until either a new job comes or
		the worker must die (workpool is stopping).
		\retval int BAD no job is returned and worker must die, OK a job was returned
		and it must be processed, NOK a job is returned but the workpool is stopping.
	*/
	int pop(int _wkrid, Jb &_jb){
		Mutex::Locker lock(mtx);
		//if(_wkrid >= wkrcnt) return BAD;
		while(q.empty() && state == Running){
			sigcnd.wait(mtx);
		}
		if(q.size()){
			_jb = q.front();
			q.pop();
			if(q.size()) sigcnd.signal();
			if(state == Running) return OK;
			return NOK;
		}
		return BAD;//the workpool is about to stop
	}
	//! Pop multiple jobs in a given table.
	/*!
		This must be called in a while loop by the concrete workpool's run method.
		If no job is available, it will block until either a new job comes or
		the worker must die (workpool is stopping).
		\retval int BAD no job is returned and worker must die, OK a job was returned
		and it must be processed, NOK a job is returned but the workpool is stopping.
		\param _wkrid The id of the worker
		\param _jb	A table of jobs
		\param _cnt Input - the capacity of the table, Output the number of returned jobs.
	*/
	int pop(int _wkrid, Jb *_jb, unsigned &_cnt){
		Mutex::Locker lock(mtx);
		idbgx(Dbg::utility, "enter pop");
		//if(_wkrid >= wkrcnt) return BAD;
		if(!_cnt){
			idbgx(Dbg::utility, "exit pop1");
			if(state == Running) return OK;
			idbgx(Dbg::utility, "exit pop2");
			return BAD;
		}
		while(q.empty() && state == Running){
			idbgx(Dbg::utility, "here");
			sigcnd.wait(mtx);
		}
		int i = 0;
		if(q.size()){
			do{
				_jb[i] = q.front();
				q.pop(); ++i;
			}while(q.size() && i < _cnt);
			_cnt = i;
			if(q.size()) sigcnd.signal();//wake another worker
			idbgx(Dbg::utility, "exit pop3 cnt = "<<_cnt);
			if(state == Running) return OK;
			idbgx(Dbg::utility, "exit pop4");
			return NOK;
		}
		idbgx(Dbg::utility, "exit pop5");
		return BAD;//the workpool is about to stop
	}
protected:
	enum States {Stopped = 0, Stopping, Running};
	enum Signals{ Init, JobEnter, JobExit, MustDie,WkrEnter,WkrExit};
	//! Constructor receiving a plugin
	WorkPool(WorkPoolPlugin *_pwp = basicWorkPoolPlugin()):state(Stopped), wkrcnt(0), pwp(_pwp){}
	//! virtual destructor
	virtual ~WorkPool(){
		//big mistacke to call stop from here
		//top destructors are called first and the workers might have references to then
		if(pwp->release()) delete pwp;
	}
	
	///usually used for initiating static data for workers
	//void prepareWorker(){}
	//void unprepareWorker(){}
	
protected:
	//! Commodity method for a creating generic worker
	template <class Wkr, class WP>
	Wkr* createWorker(WP &_wp){
		return static_cast<Wkr*>(new GenericWorker<Wkr, WP>(_wp));
	}
	//returns how many workers were created
	//! Called by the workpool when more workers are needed
	/*!	
		\retval int how many workers were actually created.
	*/
	virtual int createWorkers(uint) = 0;
protected://workers:
	//! The base class for all workers.
	class Worker: public Thread{
	public:
		virtual ~Worker(){
			idbgx(Dbg::utility, "here");
		}
		int wid(){return wkrid;}
	protected:
		void prepare(WorkPool<Jb> &_rwp){
			_rwp.pwp->prepare();
		}
		void unprepare(WorkPool<Jb> &_rwp){
			_rwp.pwp->unprepare();
		}
		friend class WorkPool<Jb>;
		int 	wkrid;//worker id
	};
	friend class Worker;
	//! The concrete class for all workers
	/*!
		This means that all the actual workers must be GenericWorker\<MyWorker, MyWorkPool\>.
		You can actually only inherit from Worker but you really must know what you are 
		doing.<br>
		This allows for the following desing.<br>
		Suppose you want to have your workers keeping some data structures accessible from 
		the workpool's run method:<br>
		- define your SomeWorker
		- define your SomeWorkPool::run(SomeWorker&)
		- in your defined createWorkers method, use createWorker\<SomeWorker, SomeWorkPool\>()
		
	*/
	template <class Wkr, class WP>
	struct GenericWorker: Wkr{
		GenericWorker(WP &_wp):wp(_wp){}
		~GenericWorker(){}
		void prepare()	{	wp.enterWorker(*this); wp.prepareWorker();	Worker::prepare(wp);}
		void run()		{	wp.run(*this);											}
		void unprepare(){	Worker::unprepare(wp);	wp.unprepareWorker(); wp.exitWorker(); 	}//NOTE: the call order is very important!!
		WP	&wp;
	};
private:
	void enterWorker(Worker &_rw){
		idbgx(Dbg::utility, "wkrenter");
		mtx.lock();
		//_rwb.workerId(wkrcnt);
		_rw.wkrid = wkrcnt;
		++wkrcnt;
		idbgx(Dbg::utility, "newwkr "<<wkrcnt);
		thrcnd.signal();
		mtx.unlock();
	}
	void exitWorker(){
		idbgx(Dbg::utility, "wkexit");
		mtx.lock();
		--wkrcnt;
		idbgx(Dbg::utility, "exitwkr "<<wkrcnt<<std::flush);
		thrcnd.signal();
		mtx.unlock();
	}
protected:
	States						state;
	int							wkrcnt;
	Condition					thrcnd;
	Condition					sigcnd;
	Mutex						mtx;
	Queue<Jb>					q;
private:
	WorkPoolPlugin				*pwp;
};


#endif



