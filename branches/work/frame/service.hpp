// frame/service.hpp
//
// Copyright (c) 2013, 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_SERVICE_HPP
#define SOLID_FRAME_SERVICE_HPP

#include "frame/common.hpp"
#include "frame/event.hpp"
#include "frame/manager.hpp"
#include "system/mutex.hpp"
#include "utility/dynamictype.hpp"
#include <vector>
#ifdef HAS_STDATOMIC
#include <atomic>
#else
#include "boost/atomic.hpp"
#endif

namespace solid{
namespace frame{

class	ObjectBase;

class Service: public Dynamic<Service>{
public:
	Service(
		Manager &_rm
	);
	~Service();
	
	bool isRegistered()const;
	
	template <class Obj, class Sch>
	ObjectUidT	registerObject(DynamicPointer<Obj> &_robjptr, Sch &_rsch, Event const &_revt, ErrorConditionT &_rerr){
		ScheduleObjectF<Obj, Sch>			fnc(_robjptr, _rsch, _revt);
		Manager::ObjectScheduleFunctorT		fctor(fnc);
		return rm.registerServiceObject(*this, *_robjptr, fctor, _rerr);
	}
	
	void notifyAll(Event const &_e, const size_t _sigmsk = 0);

	template <class N>
	bool forEach(N &_rn){
		return rm.forEachServiceObject(*this, _rn);
	}
	
	void stop(bool _wait = true);
	
	Manager& manager();
protected:
	Mutex& mutex()const;
private:
	friend class Manager;
	Manager 					&rm;
	ATOMIC_NS::atomic<size_t>	idx;
};

inline Manager& Service::manager(){
	return rm;
}
inline bool Service::isRegistered()const{
	//return idx != static_cast<size_t>(-1);
	return idx.load(/*ATOMIC_NS::memory_order_seq_cst*/) != static_cast<size_t>(-1);
}

}//namespace frame
}//namespace solid

#endif
