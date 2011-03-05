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
#include "foundation/manager.hpp"
#include "foundation/object.hpp"
#include "foundation/execscheduler.hpp"

#include "utility/workpool.hpp"

namespace foundation{

//--------------------------------------------------------------------

struct Data: WorkPoolControllerBase{
	
	typedef WorkPool<ExecScheduler::JobT, Data>	WorkPoolT;
	
	Data(ExecScheduler &_res):res(_res){}
	
	void prepareWorker(WorkerBase &_rw){
		res.prepareThread();
	}
	void unprepareWorker(WorkerBase &_rw){
		res.unprepareThread();
	}
	bool createWorker(WorkPoolT &_rwp){
		Worker	*pw(_rwp.createSingleWorker());
		if(pw && pw->start() != OK){
			delete pw;
			return false;
		}return true;
	}
	void execute(WorkerBase &_rw, ExecScheduler::JobT &_rjob);
	ExecScheduler	&res;
	WorkPoolT		wp;
};


void ExecScheduler::Data::execute(WorkerBase &_rw, ExecScheduler::JobT &_rjob){
	int rv(_rjob->execute());
	switch(rv){
		case LEAVE:
			_rjob.release();
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

/*static*/ void ExecScheduler::schedule(const JobT &_rjb, uint _idx){
	static_cast<ExecScheduler*>(m().scheduler<ExecScheduler>(_idx))->d.wp.push(_rjb);
}
	
ExecScheduler::ExecScheduler(
	uint16 _startthrcnt,
	uint32 _maxthrcnt
):SchedulerBase(_startthrcnt, _maxthrcnt, 0){
	
}
ExecScheduler::~ExecScheduler(){
	
}
	
void ExecScheduler::start(uint16 _startwkrcnt){
	d.wp.start(_startwkrcnt);
}

void ExecScheduler::stop(bool _wait){
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

