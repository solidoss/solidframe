// frame/src/execscheduler.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/schedulerbase.hpp"
#include "frame/selectorbase.hpp"
#include "frame/manager.hpp"
#include "frame/object.hpp"
#include "frame/basicscheduler.hpp"

#include "utility/workpool.hpp"

#include "system/timespec.hpp"

namespace solid{
namespace frame{

//--------------------------------------------------------------------


struct BasicScheduler::Data: WorkPoolControllerBase, SelectorBase{
	
	typedef WorkPool<BasicScheduler::JobT, Data&>	WorkPoolT;
	
	Data(BasicScheduler &_res):res(_res), wp(*this){}
	
	bool prepareWorker(WorkerBase &_rw){
		return res.prepareThread();
	}
	void unprepareWorker(WorkerBase &_rw){
		res.unprepareThread();
	}
	bool createWorker(WorkPoolT &_rwp, ushort _wkrcnt){
		WorkerBase	*pw(_rwp.createSingleWorker());
		if(pw && !pw->start()){
			delete pw;
			return false;
		}return true;
	}
	void onPush(WorkPoolT &_rwp){
		if(res.crtwkrcnt < res.maxwkrcnt){
			_rwp.createWorker();
		}
	}
	/*virtual*/ void raise(uint32 _objidx = 0){}
	
	void execute(WorkPoolBase &_rwp, WorkerBase &_rw, BasicScheduler::JobT &_rjob);
	BasicScheduler	&res;
	WorkPoolT		wp;
	
};


void BasicScheduler::Data::execute(WorkPoolBase &_rwp, WorkerBase &_rw, BasicScheduler::JobT &_rjob){
	TimeSpec					ts;
	Object::ExecuteController	exectl(0, ts);
	
	SelectorBase::executeObject(*_rjob, exectl);
	
	switch(exectl.returnValue()){
		case Object::ExecuteContext::LeaveRequest:
			_rjob.clear();
			break;
		case Object::ExecuteContext::RescheduleRequest:
			wp.push(_rjob);
			break;
		case Object::ExecuteContext::CloseRequest:
			SelectorBase::stopObject(*_rjob);
			_rjob.clear();
			break;
		default:
			edbg("Unknown return value from Object::exec "<<exectl.returnValue());
			_rjob.clear();
			break;
	}
}

//--------------------------------------------------------------------

// void BasicScheduler::schedule(const JobT &_rjb){
// 	d.wp.push(_rjb);
// }
	
BasicScheduler::BasicScheduler(
	Manager &_rm,
	uint16 _startthrcnt,
	uint32 _maxthrcnt
):SchedulerBase(_rm, _startthrcnt, _maxthrcnt, 0), d(*(new Data(*this))){
	
}
BasicScheduler::~BasicScheduler(){
	delete &d;
}
	
void BasicScheduler::start(uint16 _startwkrcnt){
	d.wp.start(_startwkrcnt);
}

void BasicScheduler::stop(bool _wait){
	d.wp.stop(_wait);
}

// ExecPool::ExecPool(uint32 _maxthrcnt){
// }
// /*virtual*/ ExecPool::~ExecPool(){
// }
// void ExecPool::raise(uint _thid){
// }
// void ExecPool::raise(uint _thid, uint _objid){
// }
// void ExecPool::poolid(uint _pid){
// }
// void ExecPool::run(Worker &_rw){
// 	ObjectPointer<Object> pobj;
// 	while(!pop(_rw.wid(), pobj)){
// 		idbg(_rw.wid()<<" is processing "<<pobj->id());
// 		int rv(pobj->execute());
// 		switch(rv){
// 			case LEAVE:
// 				pobj.release();
// 				break;
// 			case OK:
// 				this->push(pobj);
// 				break;
// 			case BAD:
// 				pobj.clear();
// 				break;
// 			default:
// 				edbg("Unknown return value from Object::exec "<<rv);
// 				pobj.clear();
// 				break;
// 		}
// 	}
// }
// void ExecPool::prepareWorker(){
// }
// void ExecPool::unprepareWorker(){
// }
// 
// int ExecPool::createWorkers(uint _cnt){
// 	for(uint i = 0; i < _cnt; ++i){
// 		Worker *pw = createWorker<Worker>(*this);//new MyWorkerT(*this);
// 		pw->start(true);//wait for start
// 	}
// 	return _cnt;
// }

}//namespace frame
}//namespace solid

