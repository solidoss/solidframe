// frame/src/execscheduler.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <vector>
#include "frame/schedulerbase.hpp"
#include "frame/reactorbase.hpp"
#include "frame/manager.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "utility/queue.hpp"

namespace solid{
namespace frame{

typedef Queue<UidT>		UidQueueT;

struct ReactorStub{
	ReactorStub(ReactorBase *_psel = NULL):psel(_psel), cnt(0){}
	ReactorBase		*psel;
	size_t			cnt;
	size_t			crtidx;
	UidQueueT		freeuidq;
};

typedef std::vector<ReactorStub>	ReactorVectorT;

enum StateE{
	StateStopped,
	StateError,
	StateStarting,
	StateRunning,
	StateStopping,
	StateStoppingError,
};

struct SchedulerBase::Data{
	Data(Manager &_rm):rm(_rm), crtselidx(0), selchunkcp(0), selcnt(0), state(StateStopped){
		crtminchunkcp = selchunkcp;
	}
	
	bool isTransientState()const{
		return state == StateStarting || state == StateStopping || state == StateStoppingError;
	}
	
	
	Manager				&rm;
	size_t				crtselidx;
	size_t				selchunkcp;
	size_t				crtminchunkcp;
	size_t				selcnt;
	StateE				state;
	ReactorVectorT		reactvec;
	Mutex				mtx;
	Condition			cnd;
};


SchedulerBase::SchedulerBase(
	Manager &_rm
):d(*(new Data(_rm))){
}

SchedulerBase::~SchedulerBase(){
	delete &d;
}

bool SchedulerBase::update(ReactorBase &_rsel){
	Locker<Mutex>	lock(d.mtx);
	ReactorStub		&rselstb = d.reactvec[_rsel.schidx];
	
	for(UidVectorT::const_iterator it(_rsel.freeuidvec.begin()); it != _rsel.freeuidvec.end(); ++it){
		rselstb.freeuidq.push(*it);
	}
	rselstb.cnt -= _rsel.freeuidvec.size();
	_rsel.freeuidvec.clear();
	
	const size_t	chunkcp = ((rselstb.cnt / d.selchunkcp) + 1) * d.selchunkcp;
	
	if(chunkcp < d.crtminchunkcp/* && rselstb.cnt < (chunkcp / 2)*/){
		d.crtselidx = _rsel.schidx;
		d.crtminchunkcp = chunkcp;
	}
	_rsel.update();
	return true;
}

bool SchedulerBase::doStart(CreateWorkerF _pf, size_t _reactorcnt/* = 1*/, size_t _reactorchunkcp/* = 1024*/){
	if(_reactorcnt == 0){
		_reactorcnt = Thread::processorCount();
	}
	Locker<Mutex>	lock(d.mtx);
	
	while(d.isTransientState()){
		d.cnd.wait(lock);
	}
	
	if(d.state == StateRunning){
		return true;
	}
	d.state = StateStarting;
	d.selchunkcp = _reactorchunkcp;
	
	for(size_t i = 0; i < _reactorcnt; ++i){
		if((*_pf)(*this)){
			++d.selcnt;
		}else{
			d.state = StateStoppingError;
			break;
		}
	}
	if(d.state == StateStoppingError){
		while(d.selcnt){
			d.cnd.wait(lock);
		}
		d.state = StateError;
	}else{
		while(d.reactvec.size() != d.selcnt && d.state == StateStarting){
			d.cnd.wait(lock);
		}
		if(d.reactvec.size() != d.selcnt){
			for(ReactorVectorT::const_iterator it(d.reactvec.begin()); it != d.reactvec.end(); ++it){
				if(it->psel){
					it->psel->stop();
				}
			}
			while(d.selcnt){
				d.cnd.wait(lock);
			}
			d.state = StateError;
		}else if(d.reactvec.size() == d.selcnt){
			d.state = StateRunning;
			return true;
		}
	}
	return false;
}

void SchedulerBase::doStop(bool _wait/* = true*/){
	Locker<Mutex>	lock(d.mtx);
	while(d.isTransientState() || d.selcnt){
		d.cnd.wait(lock);
	}
	if(_wait && d.selcnt == 0 && d.state == StateRunning){
		d.state = StateStopped;
	}
	if(d.state == StateStopped || d.state == StateError){
		return;
	}
	d.state = StateStopping;
	for(ReactorVectorT::const_iterator it(d.reactvec.begin()); it != d.reactvec.end(); ++it){
		if(it->psel){
			it->psel->stop();
		}
	}
	if(_wait){
		while(d.selcnt){
			d.cnd.wait(lock);
		}
		d.state = StateStopped;
	}
}

bool SchedulerBase::doSchedule(ObjectBase &_robj, ScheduleFunctorT &_rfct){
	Locker<Mutex>	lock(d.mtx);
	if(d.state == StateRunning){
		
	}else{
		while(d.isTransientState()){
			d.cnd.wait(lock);
		}
		if(d.state != StateRunning){
			return false;
		}
	}
	if(d.reactvec[d.crtselidx].cnt < d.crtminchunkcp){
		
	}else{
		size_t mincp = -1;
		size_t idx = 0;
		for(ReactorVectorT::const_iterator it(d.reactvec.begin()); it != d.reactvec.end(); ++it){
			if(it->cnt < mincp){
				idx = d.reactvec.end() - it;
				mincp = it->cnt;
			}
		}
		d.crtminchunkcp = ((mincp / d.selchunkcp) + 1) * d.selchunkcp;
		d.crtselidx = idx;
	}
	ReactorStub	&rsel = d.reactvec[d.crtselidx];
	UidT			uid(rsel.crtidx, 0);
	if(rsel.freeuidq.size()){
		uid = rsel.freeuidq.front();
		rsel.freeuidq.pop();
	}else{
		uid.index = d.rm.computeThreadId(rsel.psel->mgridx, uid.index);
		++rsel.crtidx;
	}
	if(uid.isValid()){
		rsel.psel->setObjectThread(_robj, uid);
		bool rv = _rfct(*rsel.psel);
		if(rv){
			return true;
		}
		rsel.freeuidq.push(uid);
	}
	return false;
}

bool SchedulerBase::prepareThread(ReactorBase &_rs){
	Locker<Mutex>	lock(d.mtx);
	if(d.state == StateStoppingError){
		--d.selcnt;
		if(d.selcnt == 0){
			d.cnd.broadcast();
		}
		return false;
	}else if(d.rm.prepareThread(&_rs)){
		_rs.idInScheduler(d.reactvec.size());
		d.reactvec.push_back(ReactorStub(&_rs));
		if(d.reactvec.size() == d.selcnt){
			d.cnd.broadcast();
		}
		return true;
	}else{
		d.state = StateStoppingError;
		return false;
	}
}

void SchedulerBase::unprepareThread(ReactorBase &_rs){
	Locker<Mutex>	lock(d.mtx);
	d.rm.unprepareThread(&_rs);
	--d.selcnt;
	d.reactvec[_rs.idInScheduler()].psel = NULL;
	d.reactvec[_rs.idInScheduler()].cnt = 0;
	if(d.selcnt == 0){
		d.reactvec.clear();
		d.cnd.broadcast();
	}
}


}//namespace frame
}//namespace solid
