/* Declarations file selectpool.h
	
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

#ifndef SELECTPOOL_H
#define SELECTPOOL_H

#include <deque>
#include "system/debug.h"

#include "utility/workpool.h"
#include "utility/list.h"

#include "server.h"
#include "activeset.h"


namespace clientserver{


template <class Sel>
class SelectPool: public WorkPool<typename Sel::ObjectTp>, public ActiveSet{
public:
	typedef Sel									SelectorTp;
	typedef typename Sel::ObjectTp				JobTp;
	typedef SelectPool<Sel>						ThisTp;
	typedef WorkPool<typename Sel::ObjectTp>	WorkPoolTp;
private:
	//typedef std::pair<ulong, int> 					LstPairTp;
	typedef List<ulong>								ListTp;
	typedef std::pair<SelectorTp*,ListTp::iterator>	VecPairTp;
	//deque because this way raise may not be guarded
	typedef std::deque<VecPairTp>		 			SlotVecTp;
protected:
	struct SelectorWorker;
public://definition

	SelectPool(Server &_rsrv, uint _maxthcnt, uint _selcap = 1024):rsrv(_rsrv),selcap(_selcap){
		thrid = _rsrv.registerActiveSet(*this);
		thrid <<= 16;
	}
	void raise(uint _thid){
		assert(_thid < slotvec.size());
		//attention: slotvec must be a deque so I do not need a gurd
		slotvec[_thid].first->signal();
	}
	void raise(uint _thid, uint _thpos){
		assert(_thid < slotvec.size());
		slotvec[_thid].first->signal(_thpos);
	}
	void raiseStop(){
		{
			Mutex::Locker	lock(this->mut);
			for(typename SlotVecTp::iterator it(slotvec.begin()); it != slotvec.end(); ++it){
				it->first->signal();
			}
		}
		this->stop();
	}
	
	void poolid(uint _pid){Mutex::Locker	lock(this->mut); thrid = _pid << 16; }
	void prepareWorker(){rsrv.prepareThread(); doPrepareWorker();}
	void unprepareWorker(){doUnprepareWorker();	rsrv.unprepareThread();}
	void run(SelectorWorker &_rwkr){
		_rwkr.sel.prepare();
		while(pop(_rwkr.sel,_rwkr.thrid) >= 0){
			//idbg("before run");
			_rwkr.sel.run();
			//idbg("after run");
		}
		_rwkr.sel.unprepare();
		unregisterSelector(_rwkr.sel, _rwkr.thrid);
		idbg("after unregister selector");
	}
	/**
	 * Overwrite the method from WorkPoolTp, 
	 * adding logic for creating new workers.
	 */
	void push(const JobTp &_rjb){
		Mutex::Locker	lock(this->mut);
		assert(this->state != WorkPoolTp::Stopped);
		this->q.push(_rjb); this->sigcnd.signal();
		if(sgnlst.size()){
			//idbg("sgnlst.size = "<<sgnlst.size()<<" sgnlst.back = "<<sgnlst.back());
			slotvec[sgnlst.back()].first->signal();
		}else{
			//idbg("create worker");
			createWorkers(1);//increase the capacity
		}
	}
protected:
	virtual void doPrepareWorker(){}
	virtual void doUnprepareWorker(){}
protected:
	struct SelectorWorker: WorkPoolTp::Worker{
		SelectorWorker(){}
		~SelectorWorker(){}
		uint 		thrid;
		SelectorTp	sel;
	};
	int createWorkers(uint _cnt){
		uint i(0);
		for(; i < _cnt; ++i){
			SelectorWorker *pwkr = this->template createWorker<SelectorWorker,ThisTp>(*this);
			if(!registerSelector(pwkr->sel, pwkr->thrid))
				pwkr->start();//do not wait
			else{ delete pwkr; break;}
		}
		return (int)i;
	}
	int registerSelector(SelectorTp &_rs, uint &_wkrid){
		//Mutex::Locker	lock(mut);
		if(_rs.reserve(selcap)) return BAD;
		_wkrid = thrid++;
		cap += _rs.capacity();
		uint wid = _wkrid & 0xffff;
		idbg("wid = "<<wid<<" slotvec.size() = "<<slotvec.size());
		if(wid >= slotvec.size()){
			slotvec.push_back(VecPairTp(&_rs, sgnlst.insert(sgnlst.end(),wid)));
		}else{
			slotvec[wid] = VecPairTp(&_rs, sgnlst.insert(sgnlst.end(),wid));
		}
		idbg("sgnlst.size = "<<sgnlst.size()<<" sgnlst.back = "<<sgnlst.back());
		return OK;
	}
	void unregisterSelector(SelectorTp &_rs, ulong _wkrid){
		Mutex::Locker	lock(this->mut);
		cap -= _rs.capacity();
		slotvec[_wkrid & 0xffff] = VecPairTp(NULL, sgnlst.end());
	}
	int pop(SelectorTp &_rsel, uint _wkrid){
		Mutex::Locker lock(this->mut);
		int tid = _wkrid & 0xffff;
		if((this->q.empty() && !_rsel.empty())){
			if(slotvec[tid].second == sgnlst.end()){
				//register that we're not empty
				slotvec[tid].second = sgnlst.insert(sgnlst.end(), tid);
			}
			return OK;
		}
		if(_rsel.full()){
			doEnsureWorkerNotRaise(tid);
			if(this->q.size()) doTrySignalOtherWorker();
			return OK;
		}
		if(doWaitJob()){
			//we have at least one job
			do{
				_rsel.push(this->q.front(), _wkrid); this->q.pop();
			}while(this->q.size() && !_rsel.full());
			
			if(this->q.size()){//the job queue is not empty..
				doEnsureWorkerNotRaise(tid);
				doTrySignalOtherWorker();
			}
			if(this->state == WorkPoolTp::Running) return OK;
			return NOK;
		}
		return BAD;
	}
private:
	/**
	 * Make sure that the worker will not be raised again.
	 */
	inline void doEnsureWorkerNotRaise(int _thndx){
		if(slotvec[_thndx].second != sgnlst.end()){
			sgnlst.erase(slotvec[_thndx].second);
			slotvec[_thndx].second = sgnlst.end();
		}
	}
	
	int doWaitJob(){
		while(this->q.empty() && this->state == WorkPoolTp::Running){
			this->sigcnd.wait(this->mut);
		}
		return this->q.size();
	}
	inline void doTrySignalOtherWorker(){
		if(sgnlst.size()){
			//maybe some other worker can take the job
			raise(sgnlst.back());
		}else{
			//TODO: it might not be safe - create unneeded threads
			createWorkers(1);
		}
	}
private:
	ListTp			sgnlst;
	SlotVecTp		slotvec;
	ulong			cap;//the total count of objects already in pool
	uint			thrid;
	Server			&rsrv;
	uint			selcap;
	
};

}//namesspace clientserver

#endif

