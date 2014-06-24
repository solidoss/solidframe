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

class	Manager;
class	ObjectBase;

class Service: public Dynamic<Service>{
public:
	Service(
		Manager &_rm
	);
	~Service();
	
	ObjectUidT registerObject(ObjectBase &_robj);
	
	bool isRegistered()const;
	
	bool notifyAll(SharedEvent const &_e);

	template <class N>
	bool forEachObject(N &_rn){
		if(isRegistered()){
			return rm.forEachServiceObject(*this, _rn);
		}else{
			return false;
		}
	}
	
	void reset();
	
	void stop(bool _wait = true);
	
	Manager& manager();
	
	Mutex& mutex()const;
protected:
	Mutex& mutex(const IndexT &_rfullid)const;
	ObjectBase* object(const IndexT &_rfullid)const;
	ObjectUidT unsafeRegisterObject(ObjectBase &_robj)const;
	void unsafeStop(Locker<Mutex> &_rlock, bool _wait);
	void unsafeReset(Locker<Mutex> &_rlock);
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
