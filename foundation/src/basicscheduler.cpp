/* Implementation file execscheduler.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "foundation/schedulerbase.hpp"
#include "foundation/selectorbase.hpp"
#include "foundation/manager.hpp"
#include "foundation/object.hpp"
#include "foundation/basicscheduler.hpp"

#include "utility/workpool.hpp"

#include "system/timespec.hpp"

namespace foundation{

//--------------------------------------------------------------------


struct BasicScheduler::Data: WorkPoolControllerBase, SelectorBase{
	
	typedef WorkPool<BasicScheduler::JobT, Data&>	WorkPoolT;
	
	Data(BasicScheduler &_res):res(_res), wp(*this){}
	
	void prepareWorker(WorkerBase &_rw){
		res.prepareThread();
	}
	void unprepareWorker(WorkerBase &_rw){
		res.unprepareThread();
	}
	bool createWorker(WorkPoolT &_rwp){
		WorkerBase	*pw(_rwp.createSingleWorker());
		if(pw && pw->start() != OK){
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
	
	void execute(WorkerBase &_rw, BasicScheduler::JobT &_rjob);
	BasicScheduler	&res;
	WorkPoolT		wp;
	
};


void BasicScheduler::Data::execute(WorkerBase &_rw, BasicScheduler::JobT &_rjob){
	TimeSpec ts;
	int rv(SelectorBase::executeObject(*_rjob, 0, ts));
	switch(rv){
		case LEAVE:
			_rjob.clear();
			break;
		case OK:
			wp.push(_rjob);
			break;
		case BAD:
			_rjob.clear();
			break;
		default:
			edbg("Unknown return value from Object::exec "<<rv);
			_rjob.clear();
			break;
	}
}

//--------------------------------------------------------------------

/*static*/ void BasicScheduler::schedule(const JobT &_rjb, uint _idx){
	static_cast<BasicScheduler*>(m().scheduler<BasicScheduler>(_idx))->d.wp.push(_rjb);
}
	
BasicScheduler::BasicScheduler(
	uint16 _startthrcnt,
	uint32 _maxthrcnt
):SchedulerBase(_startthrcnt, _maxthrcnt, 0), d(*(new Data(*this))){
	
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

}//namespace

