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
	ReactorStub(ReactorBase *_preactor = NULL):preactor(_preactor), cnt(0){}
	ReactorBase		*preactor;
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
	Data(Manager &_rm):rm(_rm), crtreactoridx(0), reactorchunkcp(0), reactorcnt(0), state(StateStopped){
		crtminchunkcp = reactorchunkcp;
	}
	
	bool isTransientState()const{
		return state == StateStarting || state == StateStopping || state == StateStoppingError;
	}
	
	
	Manager				&rm;
	size_t				crtreactoridx;
	size_t				reactorchunkcp;
	size_t				crtminchunkcp;
	size_t				reactorcnt;
	StateE				state;
	ReactorVectorT		reactorvec;
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

Manager& SchedulerBase::manager(){
	return d.rm;
}

bool SchedulerBase::update(ReactorBase &_rreactor){
	Locker<Mutex>	lock(d.mtx);
	ReactorStub		&rreactorstb = d.reactorvec[_rreactor.schidx];
	
	for(UidVectorT::const_iterator it(_rreactor.freeuidvec.begin()); it != _rreactor.freeuidvec.end(); ++it){
		rreactorstb.freeuidq.push(*it);
	}
	rreactorstb.cnt -= _rreactor.freeuidvec.size();
	_rreactor.freeuidvec.clear();
	
	const size_t	chunkcp = ((rreactorstb.cnt / d.reactorchunkcp) + 1) * d.reactorchunkcp;
	
	if(chunkcp < d.crtminchunkcp/* && rselstb.cnt < (chunkcp / 2)*/){
		d.crtreactoridx = _rreactor.schidx;
		d.crtminchunkcp = chunkcp;
	}
	_rreactor.update();
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
	d.reactorchunkcp = _reactorchunkcp;
	
	for(size_t i = 0; i < _reactorcnt; ++i){
		if((*_pf)(*this)){
			++d.reactorcnt;
		}else{
			d.state = StateStoppingError;
			break;
		}
	}
	if(d.state == StateStoppingError){
		while(d.reactorcnt){
			d.cnd.wait(lock);
		}
		d.state = StateError;
	}else{
		while(d.reactorvec.size() != d.reactorcnt && d.state == StateStarting){
			d.cnd.wait(lock);
		}
		if(d.reactorvec.size() != d.reactorcnt){
			for(ReactorVectorT::const_iterator it(d.reactorvec.begin()); it != d.reactorvec.end(); ++it){
				if(it->preactor){
					it->preactor->stop();
				}
			}
			while(d.reactorcnt){
				d.cnd.wait(lock);
			}
			d.state = StateError;
		}else if(d.reactorvec.size() == d.reactorcnt){
			d.state = StateRunning;
			return true;
		}
	}
	return false;
}

void SchedulerBase::doStop(bool _wait/* = true*/){
	Locker<Mutex>	lock(d.mtx);
	while(d.isTransientState() || d.reactorcnt){
		d.cnd.wait(lock);
	}
	if(_wait && d.reactorcnt == 0 && d.state == StateRunning){
		d.state = StateStopped;
	}
	if(d.state == StateStopped || d.state == StateError){
		return;
	}
	d.state = StateStopping;
	for(ReactorVectorT::const_iterator it(d.reactorvec.begin()); it != d.reactorvec.end(); ++it){
		if(it->preactor){
			it->preactor->stop();
		}
	}
	if(_wait){
		while(d.reactorcnt){
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
	if(d.reactorvec[d.crtreactoridx].cnt < d.crtminchunkcp){
		
	}else{
		size_t mincp = -1;
		size_t idx = 0;
		for(ReactorVectorT::const_iterator it(d.reactorvec.begin()); it != d.reactorvec.end(); ++it){
			if(it->cnt < mincp){
				idx = d.reactorvec.end() - it;
				mincp = it->cnt;
			}
		}
		d.crtminchunkcp = ((mincp / d.reactorchunkcp) + 1) * d.reactorchunkcp;
		d.crtreactoridx = idx;
	}
	
	ReactorStub		&rreactor = d.reactorvec[d.crtreactoridx];
	UidT			uid(rreactor.crtidx, 0);
	
	if(rreactor.freeuidq.size()){
		uid = rreactor.freeuidq.front();
		rreactor.freeuidq.pop();
	}else{
		uid.index = d.rm.computeThreadId(rreactor.preactor->mgridx, uid.index);
		++rreactor.crtidx;
	}
	if(uid.isValid()){
		rreactor.preactor->setObjectThread(_robj, uid);
		bool rv = _rfct(*rreactor.preactor);
		if(rv){
			return true;
		}
		rreactor.freeuidq.push(uid);
	}
	return false;
}

bool SchedulerBase::prepareThread(ReactorBase &_rreactor){
	Locker<Mutex>	lock(d.mtx);
	
	if(d.state == StateStoppingError){
		--d.reactorcnt;
		if(d.reactorcnt == 0){
			d.cnd.broadcast();
		}
		return false;
	}else if(d.rm.prepareThread(&_rreactor)){
		_rreactor.idInScheduler(d.reactorvec.size());
		d.reactorvec.push_back(ReactorStub(&_rreactor));
		if(d.reactorvec.size() == d.reactorcnt){
			d.cnd.broadcast();
		}
		return true;
	}else{
		d.state = StateStoppingError;
		return false;
	}
}

void SchedulerBase::unprepareThread(ReactorBase &_rreactor){
	Locker<Mutex>	lock(d.mtx);
	
	d.rm.unprepareThread(&_rreactor);
	--d.reactorcnt;
	d.reactorvec[_rreactor.idInScheduler()].preactor = NULL;
	d.reactorvec[_rreactor.idInScheduler()].cnt = 0;
	if(d.reactorcnt == 0){
		d.reactorvec.clear();
		d.cnd.broadcast();
	}
}


}//namespace frame
}//namespace solid
