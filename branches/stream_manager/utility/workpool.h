/* Declarations file workpool.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef WORKPOOL_H
#define WORKPOOL_H

#include "system/debug.h"
#include "system/thread.h"
#include "system/synchronization.h"

#include "common.h"
#include "queue.h"

struct WorkPoolPlugin{
	virtual ~WorkPoolPlugin();
	virtual void prepare();
	virtual void unprepare();
	virtual bool release()const;
};

WorkPoolPlugin* basicWorkPoolPlugin();

template <class Jb>
class WorkPool{
public:
	typedef WorkPool<Jb>	WorkPoolTp;
	void push(const Jb& _jb){
		mut.lock();
		assert(state != Stopped);
		q.push(_jb);
		sigcnd.signal();
		mut.unlock();
	}
	void push(const Jb *_pjb, unsigned _cnt){
		if(!_cnt) return;
		mut.lock();
		assert(state != Stopped);
		while(_cnt--){
			q.push(*_pjb);
			++_pjb;
		}
		sigcnd.signal();
		mut.unlock();
	}
	int tryPush(const Jb &_jb){
		if(state < Running) return BAD;
		push(_jb);
		return OK;
	}
	int tryPush(const Jb *_pjb, int _cnt){
		if(state < Running) return BAD;
		push(_pjb,_cnt);
		return OK;
	}
	virtual int start(ushort _minwkrcnt){
		Mutex::Locker lock(mut);
		if(state > Stopped) return BAD;
		_minwkrcnt = (_minwkrcnt)?_minwkrcnt:1;
		wkrcnt = 0;
		state = Running;
		idbg("before create workers");
		createWorkers(_minwkrcnt);
		idbg("after create");
		while(wkrcnt != _minwkrcnt) thrcnd.wait(mut);
		return OK;
	}
	virtual void stop(){
		Mutex::Locker	lock(mut);
		if(state != Running) return;
		idbg("workpool::stopping q.size"<<q.size());
		state = Stopping;
		sigcnd.broadcast();
		while(wkrcnt)	thrcnd.wait(mut);
		idbg("workpool::stopped"<<std::flush);
		state = Stopped;
	}
protected:
	int pop(int _wkrid, Jb &_jb){
		Mutex::Locker lock(mut);
		//if(_wkrid >= wkrcnt) return BAD;
		while(q.empty() && state == Running){
			sigcnd.wait(mut);
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
	
	int pop(int _wkrid, Jb *_jb, unsigned &_cnt){
		Mutex::Locker lock(mut);
		idbg("enter pop");
		//if(_wkrid >= wkrcnt) return BAD;
		if(!_cnt){
			idbg("exit pop1");
			if(state == Running) return OK;
			idbg("exit pop2");
			return BAD;
		}
		while(q.empty() && state == Running){
			idbg("here");
			sigcnd.wait(mut);
		}
		int i = 0;
		if(q.size()){
			do{
				_jb[i] = q.front();
				q.pop(); ++i;
			}while(q.size() && i < _cnt);
			_cnt = i;
			if(q.size()) sigcnd.signal();//wake another worker
			idbg("exit pop3 cnt = "<<_cnt);
			if(state == Running) return OK;
			idbg("exit pop4");
			return NOK;
		}
		idbg("exit pop5");
		return BAD;//the workpool is about to stop
	}
protected:
	enum States {Stopped = 0, Stopping, Running};
	enum Signals{ Init, JobEnter, JobExit, MustDie,WkrEnter,WkrExit};
	WorkPool(WorkPoolPlugin *_pwp = basicWorkPoolPlugin()):state(Stopped), wkrcnt(0), pwp(_pwp){}
	
	virtual ~WorkPool(){
		//big mistacke to call stop from here
		//top destructors are called first and the workers might have references to then
		if(pwp->release()) delete pwp;
	}
	
	///usually used for initiating static data for workers
	//void prepareWorker(){}
	//void unprepareWorker(){}
	
protected:
	template <class Wkr, class WP>
	Wkr* createWorker(WP &_wp){
		return static_cast<Wkr*>(new GenericWorker<Wkr, WP>(_wp));
	}
	//returns how many workers were created
	virtual int createWorkers(uint) = 0;
protected://workers:
	class Worker: public Thread{
	public:
		virtual ~Worker(){
			idbg("here");
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
		idbg("wkrenter");
		mut.lock();
		//_rwb.workerId(wkrcnt);
		_rw.wkrid = wkrcnt;
		++wkrcnt;
		idbg("newwkr "<<wkrcnt);
		thrcnd.signal();
		mut.unlock();
	}
	void exitWorker(){
		idbg("wkexit");
		mut.lock();
		--wkrcnt;
		idbg("exitwkr "<<wkrcnt<<std::flush);
		thrcnd.signal();
		mut.unlock();
	}
protected:
	States						state;
	int							wkrcnt;
	Condition					thrcnd;
	Condition					sigcnd;
	Mutex						mut;
	Queue<Jb>					q;
private:
	WorkPoolPlugin				*pwp;
};


#endif



