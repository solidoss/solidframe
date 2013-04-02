/* Implementation file consensusregistrar.cpp
	
	Copyright 2013 Valentin Palade 
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

#include "frame/common.hpp"
#include "consensus/consensusregistrar.hpp"
#include "system/mutex.hpp"

#include <vector>

namespace solid{
namespace consensus{

struct Registrar::Data{
	struct Stub{
		Stub():objuid(frame::invalid_uid()){}
		frame::ObjectUidT	objuid;
	};	
	typedef std::vector<Stub>	ObjectUidVectorT;
	
	Data():insertidx(0){}
	
	Mutex				mtx;
	ObjectUidVectorT	objuidvec;
	size_t				insertidx;
};

/*static*/ Registrar& Registrar::the(){
	static Registrar r;
	return r;
}

frame::IndexT  Registrar::registerObject(const frame::ObjectUidT &_robjuid, const frame::IndexT &_ridx){
	Locker<Mutex> lock(d.mtx);
	if(frame::is_valid_index(_ridx)){
		if(_ridx >= d.objuidvec.size()){
			d.objuidvec.resize(_ridx + 1);
		}
		if(frame::is_invalid_uid(d.objuidvec[_ridx].objuid)){
			d.objuidvec[_ridx].objuid = _robjuid;
			return _ridx;
		}else{
			return INVALID_INDEX;
		}
	}
	while(d.insertidx < d.objuidvec.size() && frame::is_valid_uid(d.objuidvec[d.insertidx].objuid)){
		++d.insertidx;
	}
	if(d.insertidx == d.objuidvec.size()){
		d.objuidvec.push_back(Data::Stub());
	}
	d.objuidvec[d.insertidx].objuid = _robjuid;
	++d.insertidx;
	return d.insertidx - 1;
}

void Registrar::unregisterObject(frame::IndexT &_ridx){
	Locker<Mutex> lock(d.mtx);
	if(_ridx < d.objuidvec.size() && frame::is_valid_uid(d.objuidvec[_ridx].objuid)){
		d.objuidvec[_ridx].objuid = frame::invalid_uid();
	}
	_ridx = INVALID_INDEX;
}

frame::ObjectUidT Registrar::objectUid(const frame::IndexT &_ridx)const{
	Locker<Mutex> lock(d.mtx);
	if(_ridx < d.objuidvec.size() && !frame::is_invalid_uid(d.objuidvec[_ridx].objuid)){
		return d.objuidvec[_ridx].objuid;
	}else{
		return frame::invalid_uid();
	}
}

Registrar::Registrar():d(*(new Data)){}

Registrar::~Registrar(){
	delete &d;
}

}//namespace consensus
}//namespace solid
