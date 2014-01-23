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

#include "frame/object.hpp"

namespace solid{
namespace frame{

class Manager;

//! The base for every selector
/*!
 * The manager will call raise when an object needs processor
 * time, e.g. because of an event.
 */
class SelectorBase{
public:
	uint32 id()const;
	virtual void raise(uint32 _objidx = 0) = 0;
protected:
	void associateObjectToCurrentThread(Object &_robj);
	bool setObjectThread(Object &_robj, const IndexT &_objidx);
	void executeObject(Object &_robj, Object::ExecuteContext &_rexectx);
	void stopObject(Object &_robj);
	void setCurrentTimeSpecific(const TimeSpec &_rtout);
	void id(uint32 _id);
private:
	friend class Manager;
	uint32	selid;//given by manager
};

inline uint32 SelectorBase::id()const{
	return selid;
}

inline void SelectorBase::associateObjectToCurrentThread(Object &_robj){
	_robj.associateToCurrentThread();
}
inline void SelectorBase::executeObject(Object &_robj, Object::ExecuteContext &_rexectx){
	_robj.execute(_rexectx);
}
inline void SelectorBase::stopObject(Object &_robj){
	_robj.stop();
}
inline void SelectorBase::id(uint32 _id){
	selid = _id;
}
inline void SelectorBase::setCurrentTimeSpecific(const TimeSpec &_rtout){
	Object::doSetCurrentTime(&_rtout);
}

}//namespace frame
}//namespace solid

#endif

