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
#include "frame/selectorbase.hpp"
#include "frame/manager.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"

namespace solid{
namespace frame{

struct SelectorStub{
	SelectorStub():psel(NULL), cnt(0){}
	SelectorBase	*psel;
	size_t			cnt;
};

typedef std::vector<SelectorStub>	SelectorVectorT;

enum StateE{
	StateStopped,
	StateError,
	StateStarting,
	StateRunning,
	StateStopping,
	StateStoppingError,
};

struct SchedulerBase::Data{
	Data(Manager &_rm):rm(_rm), crtselidx(0), crtselfree(0), selcnt(0), state(StateStopped){}
	
	Manager				&rm;
	size_t				crtselidx;
	size_t				crtselfree;
	size_t				selcnt;
	StateE				state;
	SelectorVectorT		selvec;
	Mutex				mtx;
	Condition			cnd;
};


SchedulerBase::SchedulerBase(
	Manager &_rm
):d(*(new Data)){
}

SchedulerBase::~SchedulerBase(){
	delete &d;
}

bool SchedulerBase::doStart(CreateWorkerF _pf, size_t _selcnt/* = 1*/, size_t _selchunkcp/* = 1024*/){
	if(_selcnt == 0){
		_selcnt = Thread::processorCount();
	}
	Locker<Mutex>	lock(d.mtx);
	
	while(d.state == StateStarting || d.state == StateStopping || d.state == StateStoppingError){
		d.cnd.wait(lock);
	}
	
	if(d.state == StateRunning){
		return true;
	}
	d.state = StateStarting;
	
	for(size_t i = 0; i < _selcnt; ++i){
		if((*_pf)(*this)){
			
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
		return false;
	}else{
		
	}
	
}

void SchedulerBase::doStop(bool _wait/* = true*/){
	
}

void SchedulerBase::doSchedule(ScheduleFunctorT &_rfct){
	
}

bool SchedulerBase::prepareThread(SelectorBase *_ps){
	d.rm.prepareThread(_ps);
}

void SchedulerBase::unprepareThread(SelectorBase *_ps){
	d.rm.unprepareThread(_ps);
}


}//namespace frame
}//namespace solid
