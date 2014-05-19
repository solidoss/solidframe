// frame/object.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_OBJECT_HPP
#define SOLID_FRAME_OBJECT_HPP

#include "system/timespec.hpp"

#include "frame/common.hpp"

#include "utility/dynamictype.hpp"
#include "utility/dynamicpointer.hpp"
#include <boost/concept_check.hpp>

#ifdef HAS_STDATOMIC
#include <atomic>
#else
#include "boost/atomic.hpp"
#endif

namespace solid{

class Mutex;

namespace frame{

class Manager;
class Service;
//class ObjectPointerBase;
class Message;
class SelectorBase;
class Object;


class Object: public Dynamic<Object, DynamicShared<> >{
public:
	struct ExecuteContext{
		enum RetValE{
			WaitRequest,
			WaitUntilRequest,
			RescheduleRequest,
			CloseRequest,
			LeaveRequest,
		};
		size_t eventMask()const{
			return evsmsk;
		}
		const TimeSpec& currentTime()const{
			return rcrttm;
		}
		void reschedule();
		void close();
		void leave(Object &_robj, DynamicPointer<Object> &_robjptr);
		void wait();
		void waitUntil(const TimeSpec &_rtm);
		void waitFor(const TimeSpec &_rtm);
	protected:
		ExecuteContext(
			const size_t _evsmsk,
			const TimeSpec &_rcrttm
		):	evsmsk(_evsmsk), rcrttm(_rcrttm), retval(WaitRequest){}
		
		size_t			evsmsk;
		const TimeSpec	&rcrttm;
		RetValE			retval;
		TimeSpec		waittm;
	};

	struct ExecuteController: ExecuteContext{
		ExecuteController(
			const size_t _evsmsk,
			const TimeSpec &_rcrttm
		): ExecuteContext(_evsmsk, _rcrttm){}
		
		const RetValE returnValue()const{
			return this->retval;
		}
		const TimeSpec& waitTime()const{
			return this->waittm;
		}
	};

	static const TimeSpec& currentTime();
	
	//!Get the object associated to the current thread
	/*!
	 \see Object::associateToCurrentThread
	*/ 
	static Object& specific();
	
	//! Returns true if the object is signaled
	bool notified() const;
	
	bool notified(size_t _s) const;
	
	//! Get the id of the object
	IndexT id() const;
	
	
	/**
	 * Returns true if the signal should raise the object ASAP
	 * \param _smask The signal bitmask
	 */
	bool notify(size_t _smask);
	
	//! Signal the object with a signal
	virtual bool notify(DynamicPointer<Message> &_rmsgptr);
	
protected:
	friend class Service;
	friend class Manager;
	friend class SelectorBase;
	
	//! Constructor
	Object();
	
	
	//! Grab the signal mask eventually leaving some bits set- CALL this inside lock!!
	size_t grabSignalMask(size_t _leave = 0);
	
	//! Virtual destructor
	virtual ~Object();//only objptr base can destroy an object
	
	void unregister();
	bool isRegistered()const;
	virtual void doStop();
private:
	friend struct ExecuteContext;
	static void doSetCurrentTime(const TimeSpec *_pcrtts);
	
	//! Set the id
	void id(const IndexT &_fullid);
	//! Gets the id of the thread the object resides in
	IndexT threadId()const;
	//! Assigns the object to the current thread
	/*!
		This is usualy called by the pool's Selector.
	*/
	void associateToCurrentThread();
	
	//! Executes the objects
	/*!
		This method is calle by selectpools with support for
		events and timeouts
	*/
	virtual void execute(ExecuteContext &_rexectx);
	
	virtual void leave(DynamicPointer<Object> &_robjptr);
	void stop();
	
	//! Set the thread id
	void threadId(const IndexT &_thrid);
private:
	IndexT						fullid;

	ATOMIC_NS::atomic<size_t>	smask;
	ATOMIC_NS::atomic<IndexT>	thrid;
};

inline IndexT Object::id()	const {
	return fullid;
}

#ifndef NINLINES
#include "frame/object.ipp"
#endif

}//namespace frame
}//namespace solid


#endif
