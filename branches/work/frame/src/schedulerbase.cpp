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
#include "utility/stack.hpp"
#include "arrayfind.hpp"

#include <memory>

namespace solid{
namespace frame{

namespace{
class ErrorCategory: public ERROR_NS::error_category{
public:
	enum{
		AlreadyE,
		WorkerE,
		RunningE,
		ReactorE
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
			case RunningE:
				return "Scheduler not running";
			case ReactorE:
				return "Reactor failure";
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

inline ErrorConditionT error_running(){
	return ErrorConditionT(ErrorCategory::RunningE, ec);
}

inline ErrorConditionT error_reactor(){
	return ErrorConditionT(ErrorCategory::ReactorE, ec);
}

}//namespace

typedef Queue<UidT>					UidQueueT;
typedef Stack<UidT>					UidStackT;
typedef std::unique_ptr<Thread>		ThreadPointerT;
struct ReactorStub{
	ReactorStub(ReactorBase *_preactor = NULL):preactor(_preactor), cnt(0){}
	
	void clear(){
		preactor = NULL;
		cnt = 0;
		crtidx = 0;
		while(freeuidstk.size()) freeuidstk.pop();
	}
	
	ThreadPointerT	thrptr;
	ReactorBase		*preactor;
	size_t			cnt;
	size_t			crtidx;
	UidStackT		freeuidstk;
};

typedef std::vector<ReactorStub>	ReactorVectorT;

enum Statuses{
	StatusStoppedE = 0,
	StatusStartingWaitE,
	StatusStartingErrorE,
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
		rreactorstb.freeuidstk.push(*it);
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

ErrorConditionT SchedulerBase::doStart(CreateWorkerF _pf, size_t _reactorcnt/* = 1*/){
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
		
		if(!start_err){
			d.status = StatusStartingWaitE;
			
			do{
				d.cnd.wait(lock);
			}while(d.status == StatusStartingWaitE && d.reactorcnt != d.reactorvec.size());
			
			if(d.status == StatusStartingErrorE){
				d.status = StatusStoppingWaitE;
				start_err = true;
			}
		}
		
		if(start_err){
			for(auto it = d.reactorvec.begin(); it != d.reactorvec.end(); ++it){
				if(it->preactor){
					it->preactor->stop();
				}
			}
		}else{
			d.status = StatusRunningE;
			return ErrorConditionT();
		}
	}
	
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

void SchedulerBase::doStop(bool _wait/* = true*/){
	{
		Locker<Mutex>	lock(d.mtx);
		
		if(d.status == StatusStartingWaitE || d.status == StatusStartingErrorE){
			do{
				d.cnd.wait(lock);
			}while(d.status == StatusStartingWaitE || d.status == StatusStartingErrorE);
		}
		
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
	
	if(d.status == StatusRunningE){
		ReactorStub 	&rrs = d.reactorvec[doComputeScheduleReactorIndex()];
		
		UidT			uid(rrs.crtidx, 0);
		
		if(rrs.freeuidstk.size()){
			uid = rrs.freeuidstk.top();
			rrs.freeuidstk.pop();
		}else{
			uid.index = manager().computeThreadId(rrs.preactor->idInManager(), uid.index);
			++rrs.crtidx;
		}
		
		if(uid.isValid()){
			if(_rfct(*rrs.preactor)){
				return ErrorConditionT();
			}else{
				rrs.freeuidstk.push(uid);
			}
		}
		return error_reactor();
	}else{
		return error_running();
	}
}

size_t SchedulerBase::doComputeScheduleReactorIndex(){
	switch(d.reactorvec.size()){
		case 1:
			return 0;
		case 2:{
			const std::array<size_t, 2> arr = {
				d.reactorvec[0].preactor->load(), d.reactorvec[1].preactor->load()
			};
			return find_min(arr, d.crtreactoridx);
		}
		case 3:{
			const std::array<size_t, 3> arr = {
				d.reactorvec[0].preactor->load(), d.reactorvec[1].preactor->load(), d.reactorvec[2].preactor->load()
			};
			return find_min(arr, d.crtreactoridx);
		}
		case 4:{
			const std::array<size_t, 4> arr = {
				d.reactorvec[0].preactor->load(), d.reactorvec[1].preactor->load(), d.reactorvec[2].preactor->load(), d.reactorvec[3].preactor->load()
			};
			return find_min(arr, d.crtreactoridx);
		}
		case 5:{
			const std::array<size_t, 5> arr = {
				d.reactorvec[0].preactor->load(), d.reactorvec[1].preactor->load(), d.reactorvec[2].preactor->load(), d.reactorvec[3].preactor->load(),
				d.reactorvec[4].preactor->load()
			};
			return find_min(arr, d.crtreactoridx);
		}
		case 6:{
			const std::array<size_t, 6> arr = {
				d.reactorvec[0].preactor->load(), d.reactorvec[1].preactor->load(), d.reactorvec[2].preactor->load(), d.reactorvec[3].preactor->load(),
				d.reactorvec[4].preactor->load(), d.reactorvec[5].preactor->load()
			};
			return find_min(arr, d.crtreactoridx);
		}
		case 7:{
			const std::array<size_t, 7> arr = {
				d.reactorvec[0].preactor->load(), d.reactorvec[1].preactor->load(), d.reactorvec[2].preactor->load(), d.reactorvec[3].preactor->load(),
				d.reactorvec[4].preactor->load(), d.reactorvec[5].preactor->load(), d.reactorvec[6].preactor->load()
			};
			return find_min(arr, d.crtreactoridx);
		}
		case 8:{
			const std::array<size_t, 8> arr = {
				d.reactorvec[0].preactor->load(), d.reactorvec[1].preactor->load(), d.reactorvec[2].preactor->load(), d.reactorvec[3].preactor->load(),
				d.reactorvec[4].preactor->load(), d.reactorvec[5].preactor->load(), d.reactorvec[6].preactor->load(), d.reactorvec[7].preactor->load()
			};
			return find_min(arr, d.crtreactoridx);
		}
		default:
			break;
	}
	
	const size_t	cwi = d.crtreactoridx;
	d.crtreactoridx = (cwi + 1) % d.reactorvec.size();
	return cwi;
}

bool SchedulerBase::prepareThread(const size_t _idx, ReactorBase &_rreactor){
	Locker<Mutex>	lock(d.mtx);
	ReactorStub 	&rrs = d.reactorvec[_idx];
	
	if(d.status == StatusStartingWaitE && manager().prepareThread(&_rreactor)){
		rrs.preactor = &_rreactor;
		++d.reactorcnt;
		d.cnd.broadcast();
		return true;
	}
	d.status = StatusStartingErrorE;
	d.cnd.signal();
	return false;
}

void SchedulerBase::unprepareThread(const size_t _idx, ReactorBase &_rreactor){
	Locker<Mutex>	lock(d.mtx);
	ReactorStub 	&rrs = d.reactorvec[_idx];
	manager().unprepareThread(&_rreactor);
	rrs.clear();
	--d.reactorcnt;
	d.cnd.signal();
}


}//namespace frame
}//namespace solid
