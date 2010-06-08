/* Implementation file execpool.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "foundation/execpool.hpp"
#include "foundation/object.hpp"


namespace foundation{


ExecPool::ExecPool(uint32 _maxthrcnt){
}
/*virtual*/ ExecPool::~ExecPool(){
}
void ExecPool::raise(uint _thid){
}
void ExecPool::raise(uint _thid, uint _objid){
}
void ExecPool::poolid(uint _pid){
}
void ExecPool::run(Worker &_rw){
	ObjectPointer<Object> pobj;
	while(!pop(_rw.wid(), pobj)){
		idbg(_rw.wid()<<" is processing "<<pobj->id());
		int rv(pobj->execute());
		switch(rv){
			case LEAVE:
				pobj.release();
				break;
			case OK:
				this->push(pobj);
				break;
			case BAD:
				pobj.clear();
				break;
			default:
				edbg("Unknown return value from Object::exec "<<rv);
				pobj.clear();
				break;
		}
	}
}
void ExecPool::prepareWorker(){
}
void ExecPool::unprepareWorker(){
}

int ExecPool::createWorkers(uint _cnt){
	for(uint i = 0; i < _cnt; ++i){
		Worker *pw = createWorker<Worker>(*this);//new MyWorkerT(*this);
		pw->start(true);//wait for start
	}
	return _cnt;
}

}//namespace

