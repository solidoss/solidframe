// frame/object.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_OBJECT_HPP
#define SOLID_FRAME_OBJECT_HPP

#include "system/timespec.hpp"

#include "frame/objectbase.hpp"
#include "frame/event.hpp"

#include "utility/dynamictype.hpp"
#include "utility/dynamicpointer.hpp"
#include "system/atomic.hpp"

namespace solid{
namespace frame{

struct ExecuteContext{
	~ExecuteContext();
	Event const& event()const{
		return evn;
	}
	const TimeSpec& time()const{
		return rcrttm;
	}
	void reschedule(Event const &_revn);
	void die(){
		
	}
protected:
	ExecuteContext(
		const Event &_evn,
		const TimeSpec &_rcrttm
	):	evn(_evn), rcrttm(_rcrttm){}
	
	Event			evn;
	const TimeSpec	&rcrttm;
};

class Object: public Dynamic<Object, ObjectBase>{
public:	
	static Object& specific(){
		return static_cast<Object&>(ObjectBase::specific());
	}
protected:
	//! Constructor
	Object();
private:
	virtual void execute(ExecuteContext &_rexectx) = 0;
private:
};

}//namespace frame
}//namespace solid


#endif
