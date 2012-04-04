/* Declarations file object.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FOUNDATION_OBJECT_HPP
#define FOUNDATION_OBJECT_HPP

#include "foundation/common.hpp"

#include "utility/dynamictype.hpp"
#include "utility/dynamicpointer.hpp"

class Mutex;
struct TimeSpec;

namespace foundation{

class Manager;
class Service;
class ObjectPointerBase;
class Signal;
class SelectorBase;
class Object;

struct DynamicServicePointerStore{
	void pushBack(
		const Object *_pobj,
		const uint _idx,
		const DynamicPointer<DynamicBase> &_dp
	);
	size_t size(const Object *_pobj, const uint _idx)const;
	bool isNotLast(const Object *_pobj, const uint _idx, const uint _pos)const;
	const DynamicPointer<DynamicBase>& pointer(
		const Object *_pobj,
		const uint _idx,
		const uint _pos
	)const;
	DynamicPointer<DynamicBase>& pointer(
		const Object *_pobj,
		const uint _idx,
		const uint _pos
	);
	void clear(const Object *_pobj, const uint _idx);
};

//! A pseudo-active object class
/*!
	<b>Overview:</b><br>
	A pseudo-active object is one that altough it doesnt own a thread
	it can reside within a workpool an receive processor/thread time
	due to events.
	
	So an object:
	- can receive signals
	- can receive signals
	- must reside both on a static container (foundation::Service) and 
	an ActiveSet (usually foundation::SelectPool)
	- has an associated mutex which can be requested from the service
	- can be visited using a visitor<br>
	
	<b>Usage:</b><br>
	- Inherit the object and implement execute, adding code for grabing
	the signal mask and the signals under lock.
	- Add code so that the object gets registered on service and inserted
	into a proper workpool upon its creation.
	
	<b>Notes:</b><br>
	- Every object have an associated unique id (a pair of uint32) wich 
	uniquely identifies the object within the manager both on memory and on
	time. The solidground architecture and the ones built upon it MUST NOT allow
	access to object pointers, instead they do/should do permit limited access
	using the unique id: sigaling and sending signals through the manager interface.
	- Also an object will hold information about the thread in which it is currently
	executed, so that the signaling is fast.
*/
class Object: public Dynamic<Object>{
public:
	typedef Signal	SignalT;
	
	static const TimeSpec& currentTime();
	
	//!Get the curent object associate to the current thread
	static Object& the();
	
	//! Returns true if the object is signaled
	bool signaled() const;
	
	//! Returns the state of the objec -a negative state means the object must be destroyed
	int state() const;
	
	bool signaled(ulong _s) const;
	
	//! Get the id of the object
	IndexT id() const;
	
	ObjectUidT uid()const;
	
	//! Get the associated mutex
	Mutex& mutex()const;
	
	//! Get the id of the parent service
	IndexT serviceId()const;
	
	//! Get the index of the object within service from an objectid
	IndexT index()const;
	
	/**
	 * Returns true if the signal should raise the object ASAP
	 * \param _smask The signal bitmask
	 */
	bool signal(ulong _smask);
	
	//! Signal the object with a signal
	virtual bool signal(DynamicPointer<Signal> &_rsig);
protected:
	friend class Service;
	friend class Manager;
	friend class ObjectPointerBase;
	friend class SelectorBase;
	
	//! Constructor
	Object(IndexT _fullid = 0UL);
	
	//! Sets the current state.
	/*! If your object will implement a state machine (and in an asynchronous
	environments it most certanly will) use this base state setting it to
	somethin negative on destruction
	*/
	void state(int _st);
	
	//! Grab the signal mask eventually leaving some bits set- CALL this inside lock!!
	ulong grabSignalMask(ulong _leave = 0);
	
	//! Virtual destructor
	virtual ~Object();//only objptr base can destroy an object
	
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
	virtual int execute(ulong _evs, TimeSpec &_rtout);
	
	//! Set the thread id
	void setThread(uint32 _thrid, uint32 _thrpos);
	
	//! This is called by the service after the object was registered
	/*!
	 * Some objects may keep the mutex for faster access
	 */
	virtual void init(Mutex *_pm);
private:
	//void typeId(const uint16 _tid);
	//const uint typeId()const;
	static void doSetCurrentTime(const TimeSpec *_pcrtts);
	
	//! Set the id
	void id(IndexT _fullid);
	//! Gets the id of the thread the object resides in
	void getThread(uint32 &_rthid, uint32 &_rthpos)const;
	//! Set the id given the service id and index
	void id(IndexT _srvid, IndexT _ind);
private:
	IndexT			fullid;
	volatile ulong	smask;
	volatile uint32	thrid;//the current thread which (may) execute(s) the object
	volatile uint32	thrpos;//
	uint16			usecnt;//
	uint16			dummy;//
	int32			crtstate;// < 0 -> must die
};

inline bool Object::signaled() const {
	return smask != 0;
}
	
inline int Object::state()	const {
	return crtstate;
}

inline IndexT Object::id()	const {
	return fullid;
}

}//namespace

#ifndef NINLINES
#include "foundation/object.ipp"
#endif

#endif
