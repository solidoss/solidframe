// consensus/src/consensusregistrar.cpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
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
