// frame/selectorbase.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_SELECTOR_BASE_HPP
#define SOLID_FRAME_SELECTOR_BASE_HPP

#include <deque>
#include <vector>
#include "frame/objectbase.hpp"

namespace solid{
namespace frame{

class Manager;
class SchedulerBase;

typedef std::vector<UidT>			UidVectorT;

//! The base for every selector
/*!
 * The manager will call raise when an object needs processor
 * time, e.g. because of an event.
 */
class SelectorBase{
public:
	virtual void raise(uint32 _objidx = 0) = 0;
	virtual void stop() = 0;
	virtual void update() = 0;
	
protected:
	bool setObjectThread(ObjectBase &_robj, const UidT &_uid);
	void stopObject(ObjectBase &_robj);
	SchedulerBase& scheduler(){
		static SchedulerBase	*ps;
		return *ps;
	}
	bool prepareThread();
	void unprepareThread();
private:
	friend	class Manager;
	friend	class SchedulerBase;
	void idInManager(size_t _id);
	void idInScheduler(size_t _id);
private:
	IndexT			mgridx;//
	size_t			schidx;
	UidVectorT		freeuidvec;
};


inline void SelectorBase::stopObject(ObjectBase &_robj){
	_robj.stop();
}
inline void SelectorBase::idInManager(size_t _id){
	mgridx = _id;
}
inline void SelectorBase::idInScheduler(size_t _id){
	schidx = _id;
}

}//namespace frame
}//namespace solid

#endif

