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
#include "frame/manager.hpp"
#include <mutex>
#include "utility/dynamictype.hpp"
#include <vector>
#include "frame/schedulerbase.hpp"
#ifdef SOLID_USE_STDATOMIC
#include <atomic>
#else
#include "boost/atomic.hpp"
#endif

namespace solid{
struct Event;
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
	
	std::mutex& mutex(const ObjectBase &_robj)const;
	
	bool isRunning()const;
	
protected:
	std::mutex& mutex()const;
private:
	friend class Manager;
	friend class SchedulerBase;
	
	void setRunning(){
		running = true;
	}
	
	void resetRunning(){
		running = false;
	}
	
	ObjectIdT registerObject(ObjectBase &_robj, ReactorBase &_rr, ScheduleFunctionT &_rfct, ErrorConditionT &_rerr);
private:
	
	Manager 					&rm;
	ATOMIC_NS::atomic<size_t>	idx;
	ATOMIC_NS::atomic<bool>		running;
};

inline Manager& Service::manager(){
	return rm;
}
inline bool Service::isRegistered()const{
	return idx.load(/*ATOMIC_NS::memory_order_seq_cst*/) != InvalidIndex();
}
inline bool Service::isRunning()const{
	return running;
}
}//namespace frame
}//namespace solid

#endif
