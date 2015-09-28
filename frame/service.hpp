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
#include "frame/schedulerbase.hpp"
#ifdef SOLID_USE_STDATOMIC
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
	
	
	void notifyAll(Event const &_e, const size_t _sigmsk = 0);

	template <class F>
	bool forEach(F &_rf){
		return rm.forEachServiceObject(*this, _rf);
	}
	
	void stop(const bool _wait = true);
	
	bool start();
	
	Manager& manager();
	
	Mutex& mutex(const ObjectBase &_robj)const;
	
protected:
	Mutex& mutex()const;
private:
	friend class Manager;
	friend class SchedulerBase;
	
	ObjectUidT registerObject(ObjectBase &_robj, ReactorBase &_rr, ScheduleFunctionT &_rfct, ErrorConditionT &_rerr);
private:
	
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
