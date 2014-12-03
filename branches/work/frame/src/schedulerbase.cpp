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
#include "system/cassert.hpp"
#include "utility/queue.hpp"

#include <memory>

namespace solid{
namespace frame{

namespace{
class ErrorCategory: public ERROR_NS::error_category{
public:
	enum{
		AlreadyE,
		WorkerE,
	};
private:
	const char*   name() const noexcept (true){
		return "solid::frame::Scheduler Error";
	}
	
    std::string    message(int _ev) const{
		switch(_ev){
			case AlreadyE:
				return "Already started";
			case WorkerE:
				return "Failed to start worker";
			default:
				return "Unknown";
		}
	}
};

const ErrorCategory		ec;

inline ErrorConditionT error_already(){
	return ErrorConditionT(ErrorCategory::AlreadyE, ec);
}
inline ErrorConditionT error_worker(){
	return ErrorConditionT(ErrorCategory::WorkerE, ec);
}


}//namespace

typedef Queue<UidT>					UidQueueT;
typedef std::unique_ptr<Thread>		ThreadPointerT;
struct ReactorStub{
	ReactorStub(ReactorBase *_preactor = NULL):preactor(_preactor), cnt(0){}
	ThreadPointerT	thrptr;
	ReactorBase		*preactor;
	size_t			cnt;
	size_t			crtidx;
	UidQueueT		freeuidq;
};

typedef std::vector<ReactorStub>	ReactorVectorT;

enum Statuses{
	StatusStoppedE = 0,
	StatusRunningE,
	StatusStoppingE,
	StatusStoppingWaitE
};

struct SchedulerBase::Data{
	Data(Manager &_rm):rm(_rm), crtreactoridx(0), reactorchunkcp(0), reactorcnt(0), status(StatusStoppedE){
		crtminchunkcp = reactorchunkcp;
	}
	
	Manager				&rm;
	size_t				crtreactoridx;
	size_t				reactorchunkcp;
	size_t				crtminchunkcp;
	size_t				reactorcnt;
	size_t				stopwaitcnt;
	Statuses			status;
	ReactorVectorT		reactorvec;
	Mutex				mtx;
	Condition			cnd;
};


SchedulerBase::SchedulerBase(
	Manager &_rm
):d(*(new Data(_rm))){
}

SchedulerBase::~SchedulerBase(){
	doStop(true);
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

ErrorConditionT SchedulerBase::doStart(CreateWorkerF _pf, size_t _reactorcnt/* = 1*/, size_t _reactorchunkcp/* = 1024*/){
	if(_reactorcnt == 0){
		_reactorcnt = Thread::processorCount();
	}
	bool start_err = false;
	
	{
		Locker<Mutex>	lock(d.mtx);
		
		if(d.status == StatusRunningE){
			return error_already();
		}else if(d.status != StatusStoppedE || d.stopwaitcnt){
			do{
				d.cnd.wait(lock);
			}while(d.status != StatusStoppedE || d.stopwaitcnt);
		}
		
		d.reactorvec.resize(_reactorcnt);
		
		for(size_t i = 0; i < _reactorcnt; ++i){
			ReactorStub &rrs = d.reactorvec[i];
			rrs.thrptr.reset((*_pf)(*this, i));
			if(rrs.thrptr.get()){
				if(rrs.thrptr->start(false, false)){
					start_err = true;
					break;
				}
			}
		}
		
		if(start_err){
			d.status = StatusStoppingWaitE;
		}else{
			while(d.reactorcnt){
				d.cnd.wait(lock);
			}
		}
	}
	if(start_err){
		for(auto it = d.reactorvec.begin(); it != d.reactorvec.end(); ++it){
			if(it->thrptr.get()){
				it->thrptr->join();
			}
		}
		Locker<Mutex>	lock(d.mtx);
		d.status = StatusStoppedE;
		d.cnd.broadcast();
		return error_worker();
	}
	d.status = StatusRunningE;
	return ErrorConditionT();
}

void SchedulerBase::doStop(bool _wait/* = true*/){
	{
		Locker<Mutex>	lock(d.mtx);
		if(d.status == StatusRunningE){
			d.status = _wait ? StatusStoppingWaitE : StatusStoppingE;
			for(auto it = d.reactorvec.begin(); it != d.reactorvec.end(); ++it){
				if(it->preactor){
					it->preactor->stop();
				}
			}
		}else if(d.status == StatusStoppingE){
			d.status = _wait ? StatusStoppingWaitE : StatusStoppingE;
		}else if(d.status == StatusStoppingWaitE){
			if(_wait){
				++d.stopwaitcnt;
				do{
					d.cnd.wait(lock);
				}while(d.status != StatusStoppedE);
				--d.stopwaitcnt;
				d.cnd.signal();
			}
			return;
		}else if(d.status == StatusStoppedE){
			return;
		}
	}
	if(_wait){
		for(auto it = d.reactorvec.begin(); it != d.reactorvec.end(); ++it){
			if(it->thrptr.get()){
				it->thrptr->join();
			}
		}
		Locker<Mutex>	lock(d.mtx);
		d.status = StatusStoppedE;
		d.cnd.broadcast();
	}
}

ErrorConditionT SchedulerBase::doSchedule(ObjectBase &_robj, ScheduleFunctorT &_rfct){
	Locker<Mutex>	lock(d.mtx);
	
	
	return ErrorConditionT();
}

void SchedulerBase::prepareThread(const size_t _idx, ReactorBase &_rreactor){
	Locker<Mutex>	lock(d.mtx);
	ReactorStub 	&rrs = d.reactorvec[_idx];
	
	manager().prepareThread(_rreactor);
	rrs.preactor = &_rreactor;
	++d.reactorcnt;
	d.cnd.broadcast();
#if 0
	if(d.status == StatusStoppingErrorE){
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
		d.status = StatusStoppingError;
		return false;
	}
#endif
}

void SchedulerBase::unprepareThread(const size_t _idx, ReactorBase &_rreactor){
	Locker<Mutex>	lock(d.mtx);
	ReactorStub 	&rrs = d.reactorvec[_idx];
	manager().unprepareThread(&_rreactor);
	--d.reactorcnt;
	d.cnd.broadcast();
#if 0
	d.rm.unprepareThread(&_rreactor);
	--d.reactorcnt;
	d.reactorvec[_rreactor.idInScheduler()].preactor = NULL;
	d.reactorvec[_rreactor.idInScheduler()].cnt = 0;
	if(d.reactorcnt == 0){
		d.reactorvec.clear();
		d.cnd.broadcast();
	}
#endif
}


}//namespace frame
}//namespace solid
