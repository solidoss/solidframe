/* Declarations file service.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CS_SERVICE_HPP
#define CS_SERVICE_HPP

#include "object.hpp"

namespace foundation{

class Manager;
class Visitor;
//! Static container of objects
/*!
	<b>Overview:</b><br>
	The service statically keeps objects providing capabilities to
	signal explicit objects, signal all objects and visit objects.
	
	It also provides a mutex for every contained object.
	
	Most of the interface of the foundation::Service is forwarded
	by the foundation::Manager which is much easely accessible.
	
	Services can be started and stopped but they cannot be destroyed.
	
	Stopping means deactivating the interface which means that every
	call will fail, making it easy to stop multiple cross dependent
	service not caring for the order the services would stop.
	
	Also a foundation::Service is a foundation::Object meaning
	that it can/will reside within an active container.
	
	<b>Usage:</b><br>
	- Usually you should define a base service for every service
	of your application. That base service must inherit 
	foundation::Service.
	- You also must implement the execute method either in the
	base service or on the concrete ones.
*/
class Service: public Object{
public:
	enum States{Running, Stopping, Stopped};
	virtual ~Service();
	//! Signal an object with a signal mask
	/*!
		Added the Manager parameter too because
		the function may be called also from a non pool thread,
		so the Manager will/may not be available from thread static.
	*/
	int signal(Object &_robj, ulong _sigmask);
	//! Signal an object with a signal mask
	int signal(IndexT _fullid, uint32 _uid, ulong _sigmask);
	
	//! Signal an object with a signal
	int signal(Object &_robj, DynamicPointer<Signal> &_rsig);
	//! Signal an object with a signal
	int signal(IndexT _fullid, uint32 _uid, DynamicPointer<Signal> &_rsig);
	
	//! Signal all objects with a signal mask
	void signalAll(ulong _sigmask);
	//! Signal all objects with a signal
	/*!	
		NOTE: use it with care because the signal has to be
		prepared for paralel access as the same signal pointer
		is given to all objects.
	*/
	void signalAll(DynamicPointer<Signal> &_rsig);
	//! Visit all objects
	void visit(Visitor &_rov);
	//! Get the mutex associated to an object
	Mutex& mutex(const Object &_robj);
	//! Get the unique id associated to an object
	uint32  uid(const Object &_robj)const;
	uint32  uid(const uint32 _idx)const;
	//! The service will keep a pointer to its associated mutex
	void mutex(Mutex *_pmut);
	//! Pointer to the service's mutex
	Mutex* mutex()const;
	//! Start the service
	virtual int start();
	//! Stop it eventually waiting for all objects to unregister
	virtual int stop(bool _wait = true);
	//! Not used
	virtual int execute();
	
protected:
	//! Insert an object.
	int insert(Object &_robj, IndexT _srvid);
	//! Remove an object
	void remove(Object &_robj);
	//! Get an object mutex using objects unique id
	Mutex& mutex(IndexT _fullid, uint32 _uid);
	//! Get a pointer to an object using its unique id
	/*!
		The call may fail and it should be carefully called
		from within Service's mutex lock.
		
	*/
	Object* object(IndexT _fullid, uint32 _uid);
	//! Signal all objects - the service's mutex must be locked from outside
	void doSignalAll(Manager &_rm, ulong _sigmask);
	//! Signal all objects - the service's mutex must be locked from outside
	void doSignalAll(Manager &_rm, DynamicPointer<Signal> &_rsig);
	//! Insert an object - the service's mutex must be locked from outside
	int doInsert(Object &_robj, IndexT _srvid);
	//! Constructor - forwards the parameters to the MutualObjectStore of mutexes
	Service(int _objpermutbts = 6, int _mutrowsbts = 8, int _mutcolsbts = 8);
	
	
	ulong indexStackSize()const;
	ulong indexStackTop()const;
	void indexStackPop();
	Mutex& mutexAt(ulong _idx);
	void lockForPushBack();
	void unlockForPushBack();
	void insertObject(Object &_robj, ulong _srvid);
	void appendObject(Object &_robj, ulong _srvid);
	ulong objectVectorSize()const;
private:
	const Service& operator=(const Service &);
private:
	struct Data;
	Data	&d;
};


}//namespace foundation

#endif
