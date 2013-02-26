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

#ifndef SOLID_FRAME_OBJECT_HPP
#define SOLID_FRAME_OBJECT_HPP

#include "frame/common.hpp"

#include "utility/dynamictype.hpp"
#include "utility/dynamicpointer.hpp"

#ifdef HAS_STDATOMIC
#include <atomic>
#else
#include "boost/atomic.hpp"
#endif


class Mutex;
struct TimeSpec;

namespace solid{
namespace frame{

class Manager;
class Service;
class ObjectPointerBase;
class Message;
class SelectorBase;
class Object;



class Object: public Dynamic<Object, DynamicShared<> >{
public:
	static const TimeSpec& currentTime();
	
	//!Get the curent object associated to the current thread
	static Object& specific();
	
	//! Returns true if the object is signaled
	bool notified() const;
	
	bool notified(ulong _s) const;
	
	//! Get the id of the object
	IndexT id() const;
	
	
	/**
	 * Returns true if the signal should raise the object ASAP
	 * \param _smask The signal bitmask
	 */
	bool notify(ulong _smask);
	
	//! Signal the object with a signal
	virtual bool notify(DynamicPointer<Message> &_rmsgptr);
	
protected:
	friend class Service;
	friend class Manager;
	friend class SelectorBase;
	
	//! Constructor
	Object(IndexT _fullid = 0UL);
	
	
	//! Grab the signal mask eventually leaving some bits set- CALL this inside lock!!
	ulong grabSignalMask(ulong _leave = 0);
	
	//! Virtual destructor
	virtual ~Object();//only objptr base can destroy an object
	
private:
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
	virtual int execute(ulong _evs, TimeSpec &_rtout);
	
	//! Set the thread id
	void threadId(const IndexT &_thrid);
private:
	IndexT					fullid;
#ifdef HAS_STDATOMIC
	std::atomic<ulong>		smask;
	std::atomic<IndexT>		thrid;
#else
	boost::atomic<ulong>	smask;
	boost::atomic<IndexT>	thrid;
#endif
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
