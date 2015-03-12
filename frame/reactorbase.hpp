// frame/reactorbase.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_REACTOR_BASE_HPP
#define SOLID_FRAME_REACTOR_BASE_HPP

#include "utility/stack.hpp"
#include "frame/objectbase.hpp"

namespace solid{
namespace frame{

class Manager;
class SchedulerBase;

//! The base for every selector
/*!
 * The manager will call raise when an object needs processor
 * time, e.g. because of an event.
 */
class ReactorBase{
public:
	virtual bool raise(UidT const& _robjuid, Event const& _re) = 0;
	virtual void stop() = 0;
	
	bool prepareThread(const bool _success);
	void unprepareThread();
	size_t load()const;
protected:
	typedef ATOMIC_NS::atomic<size_t> AtomicSizeT;
	
	ReactorBase(
		SchedulerBase &_rsch, const size_t _schidx, const size_t _crtidx = 0
	):rsch(_rsch), schidx(_schidx), crtidx(_crtidx){}
	
	void stopObject(ObjectBase &_robj, Manager &_rm);
	SchedulerBase& scheduler();
	UidT popUid(ObjectBase &_robj);
	void pushUid(UidT const &_ruid);
	
	AtomicSizeT		crtload;
	
	size_t runIndex(ObjectBase &_robj)const;
private:
	friend	class SchedulerBase;
	size_t idInScheduler()const;
private:
	typedef Stack<UidT>		UidStackT;
	
	SchedulerBase	&rsch;
	size_t			schidx;
	size_t			crtidx;
	UidStackT		uidstk;
};

inline SchedulerBase& ReactorBase::scheduler(){
	return rsch;
}

inline void ReactorBase::stopObject(ObjectBase &_robj, Manager &_rm){
	_robj.stop(_rm);
}

inline size_t ReactorBase::idInScheduler()const{
	return schidx;
}

inline size_t ReactorBase::load()const{
	return crtload;
}

inline void ReactorBase::pushUid(UidT const &_ruid){
	uidstk.push(_ruid);
}

inline size_t ReactorBase::runIndex(ObjectBase &_robj)const{
	return _robj.runId().index;
}

}//namespace frame
}//namespace solid

#endif

