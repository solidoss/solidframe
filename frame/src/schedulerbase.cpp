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
#include "frame/service.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/cassert.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"
#include "utility/algorithm.hpp"
#include "system/atomic.hpp"

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
typedef ATOMIC_NS::atomic<size_t>	AtomicSizeT;

struct ReactorStub{
	ReactorStub(ReactorBase *_preactor = nullptr):preactor(_preactor){}
	
	void clear(){
		preactor = nullptr;
		thrptr.reset(nullptr);
	}
	
	ThreadPointerT	thrptr;
	ReactorBase		*preactor;
};

typedef std::vector<ReactorStub>			ReactorVectorT;

enum Statuses{
	StatusStoppedE = 0,
	StatusStartingWaitE,
	StatusStartingErrorE,
	StatusRunningE,
	StatusStoppingE,
	StatusStoppingWaitE
};

typedef ATOMIC_NS::atomic<Statuses>			AtomicStatuesT;

struct SchedulerBase::Data{
	Data():crtreactoridx(0), reactorcnt(0), stopwaitcnt(0), status(StatusStoppedE), usecnt(0){
	}
	
	size_t						crtreactoridx;
	size_t						reactorcnt;
	size_t						stopwaitcnt;
	AtomicStatuesT				status;
	AtomicSizeT					usecnt;
	ThreadEnterFunctorT			threnfnc;
	ThreadExitFunctorT			threxfnc;
	
	ReactorVectorT				reactorvec;
	Mutex						mtx;
	Condition					cnd;
};


SchedulerBase::SchedulerBase(
):d(*(new Data)){
}

SchedulerBase::~SchedulerBase(){
	doStop(true);
	delete &d;
}


bool dummy_thread_enter(){
	return true;
}
void dummy_thread_exit(){
}

ErrorConditionT SchedulerBase::doStart(
	CreateWorkerF _pf,
	ThreadEnterFunctorT _enf, ThreadExitFunctorT _exf,
	size_t _reactorcnt
){
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
		
		
		if(_enf){
			d.threnfnc = _enf;
		}else{
			d.threnfnc = dummy_thread_enter;
		}
		
		if(_exf){
			d.threxfnc = _exf;
		}else{
			d.threxfnc = dummy_thread_exit;
		}
		
		for(size_t i = 0; i < _reactorcnt; ++i){
			ReactorStub &rrs = d.reactorvec[i];
			rrs.thrptr.reset((*_pf)(*this, i));
			if(rrs.thrptr.get() == nullptr || !rrs.thrptr->start(false, false)){
				start_err = true;
				break;
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
			it->clear();
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
		while(d.usecnt){
			Thread::yield();
		}
		for(auto it = d.reactorvec.begin(); it != d.reactorvec.end(); ++it){
			if(it->thrptr.get()){
				it->thrptr->join();
				it->clear();
			}
		}
		Locker<Mutex>	lock(d.mtx);
		d.status = StatusStoppedE;
		d.cnd.broadcast();
	}
}

ObjectUidT SchedulerBase::doStartObject(ObjectBase &_robj, Service &_rsvc, ScheduleFunctorT &_rfct, ErrorConditionT &_rerr){
	++d.usecnt;
	ObjectUidT	rv;
	if(d.status == StatusRunningE){
		ReactorStub 	&rrs = d.reactorvec[doComputeScheduleReactorIndex()];
		
		rv = _rsvc.registerObject(_robj, *rrs.preactor, _rfct, _rerr);
	}else{
		_rerr = error_running();
	}
	--d.usecnt;
	return rv;
}

bool less_cmp(ReactorStub const &_rrs1, ReactorStub const &_rrs2){
	return _rrs1.preactor->load() < _rrs2.preactor->load();
}

size_t SchedulerBase::doComputeScheduleReactorIndex(){
	switch(d.reactorvec.size()){
		case 1:
			return 0;
		case 2:{
			return find_cmp(d.reactorvec.begin(), less_cmp, SizeToType<2>());
		}
		case 3:{
			return find_cmp(d.reactorvec.begin(), less_cmp, SizeToType<3>());
		}
		case 4:{
			return find_cmp(d.reactorvec.begin(), less_cmp, SizeToType<4>());
		}
		case 5:{
			return find_cmp(d.reactorvec.begin(), less_cmp, SizeToType<5>());
		}
		case 6:{
			return find_cmp(d.reactorvec.begin(), less_cmp, SizeToType<6>());
		}
		case 7:{
			return find_cmp(d.reactorvec.begin(), less_cmp, SizeToType<7>());
		}
		case 8:{
			return find_cmp(d.reactorvec.begin(), less_cmp, SizeToType<8>());
		}
		default:
			break;
	}
	
	const size_t	cwi = d.crtreactoridx;
	d.crtreactoridx = (cwi + 1) % d.reactorvec.size();
	return cwi;
}

bool SchedulerBase::prepareThread(const size_t _idx, ReactorBase &_rreactor, const bool _success){
	const bool 		thrensuccess = d.threnfnc();
	{
		Locker<Mutex>	lock(d.mtx);
		ReactorStub 	&rrs = d.reactorvec[_idx];
		
		if(_success && d.status == StatusStartingWaitE && thrensuccess){
			rrs.preactor = &_rreactor;
			++d.reactorcnt;
			d.cnd.broadcast();
			return true;
		}
		
		d.status = StatusStartingErrorE;
		d.cnd.signal();
	}
	
	d.threxfnc();
	
	return false;
}

void SchedulerBase::unprepareThread(const size_t _idx, ReactorBase &_rreactor){
	{
		Locker<Mutex>	lock(d.mtx);
		ReactorStub 	&rrs = d.reactorvec[_idx];
		rrs.preactor = nullptr;
		--d.reactorcnt;
		d.cnd.signal();
	}
	d.threxfnc();
}


}//namespace frame
}//namespace solid
