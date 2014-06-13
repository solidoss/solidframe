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

#include "utility/dynamictype.hpp"
#include "utility/dynamicpointer.hpp"
#include "system/atomic.hpp"

namespace solid{
namespace frame{
	
class Object: public Dynamic<Object, DynamicShared<ObjectBase> >{
public:	
	struct ExecuteContext{
		~ExecuteContext();
		size_t event()const{
			return evn;
		}
		size_t index()const{
			return idx;
		}
		MessagePointerT& message(){
			return msgptr;
		}
		const TimeSpec& currentTime()const{
			return rcrttm;
		}
		void reschedule(size_t _evn, size_t _idx = 0);
		void close();
	protected:
		ExecuteContext(
			const size_t _evn,
			const size_t _idx,
			MessagePointerT &_rmsgptr,
			const TimeSpec &_rcrttm
		):	evn(_evn), idx(_idx), msgptr(_rmsgptr), rcrttm(_rcrttm){}
		
		size_t			evn;
		size_t			idx;
		MessagePointerT	msgptr;
		const TimeSpec	&rcrttm;
	};

	static Object& specific(){
		return static_cast<Object&>(ObjectBase::specific());
	}
protected:
	//! Constructor
	Object();
private:
	virtual void execute(ExecuteContext &_rexectx);
private:
};

}//namespace frame
}//namespace solid


#endif
