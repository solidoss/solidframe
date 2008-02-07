/* Declarations file service.h
	
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

#ifndef CS_SERVICE_H
#define CS_SERVICE_H

#include "system/synchronization.h"

#include "utility/sharedcontainer.h"

#include "object.h"

namespace clientserver{

class Server;
class Visitor;
class ObjectVector;
class IndexStack;
//! Static container of objects
/*!
	<b>Overview:</b><br>
	The service statically keeps objects providing capabilities to
	signal explicit objects, signal all objects and visit objects.
	
	It also provides a mutex for every contained object.
	
	Most of the interface of the clientserver::Service is forwarded
	by the clientserver::Server which is much easely accessible.
	
	Services can be started and stopped but they cannot be destroyed.
	
	Stopping means deactivating the interface which means that every
	call will fail, making it easy to stop multiple cross dependent
	service not caring for the order the services would stop.
	
	Also a clientserver::Service is a clientserver::Object meaning
	that it can/will reside within an active container.
	
	<b>Usage:</b><br>
	- Usually you should define a base service for every service
	of your application. That base service must inherit 
	clientserver::Service.
	- You also must implement the execute method either in the
	base service or on the concrete ones.
*/
class Service: public Object{
public:
	enum States{Running, Stopping, Stopped};
	virtual ~Service();
	//! Signal an object with a signal mask
	/*!
		Added the Server parameter too because
		the function may be called also from a non pool thread,
		so the Server will/may not be available from thread static.
	*/
	int signal(Object &_robj, Server &_rsrv, ulong _sigmask);
	//! Signal an object with a signal mask
	int signal(ulong _fullid, ulong _uid, Server &_rsrv, ulong _sigmask);
	
	//! Signal an object with a command
	int signal(Object &_robj, Server &_rsrv, CmdPtr<Command> &_cmd);
	//! Signal an object with a command
	int signal(ulong _fullid, ulong _uid, Server &_rsrv, CmdPtr<Command> &_cmd);
	
	//! Signal all objects with a signal mask
	void signalAll(Server &_rsrv, ulong _sigmask);
	//! Signal all objects with a command
	/*!	
		NOTE: use it with care because the command has to be
		prepared for paralel access as the same command pointer
		is given to all objects.
	*/
	void signalAll(Server &_rsrv, CmdPtr<Command> &_cmd);
	//! Visit all objects
	void visit(Server &_rsrv, Visitor &_rov);
	//! Get the mutex associated to an object
	Mutex& mutex(Object &_robj);
	//! Get the unique id associated to an object
	ulong  uid(Object &_robj)const;
	
	//! The service will keep a pointer to its associated mutex
	void mutex(Mutex *_pmut);
	//! Pointer to the service's mutex
	Mutex* mutex()const{return mut;}
	//! Start the service
	virtual int start(Server &_rsrv);
	//! Stop it eventually waiting for all objects to unregister
	virtual int stop(Server &_rsrv, bool _wait = true);
	//! Not used
	virtual int execute();
	
protected:
	typedef SharedContainer<Mutex>	MutexContainer;
	//! Insert an object.
	int insert(Object &_robj, ulong _srvid);
	//! Remove an object
	void remove(Object &_robj);
	//! Get an object mutex using objects unique id
	Mutex& mutex(ulong _fullid, ulong _uid);
	//! Get a pointer to an object using its unique id
	/*!
		The call may fail and it should be carefully called
		from within Service's mutex lock.
		
	*/
	Object* object(ulong _fullid, ulong _uid);
	//! Signal all objects - the service's mutex must be locked from outside
	void doSignalAll(Server &_rsrv, ulong _sigmask);
	//! Signal all objects - the service's mutex must be locked from outside
	void doSignalAll(Server &_rsrv, CmdPtr<Command> &_cmd);
	//! Insert an object - the service's mutex must be locked from outside
	int doInsert(Object &_robj, ulong _srvid);
	//! Constructor - forwards the parameters to the SharedContainer of mutexes
	Service(int _objpermutbts = 6, int _mutrowsbts = 8, int _mutcolsbts = 8);
	//Service(const Service &):state(Stopped),objv(*((ObjectVector*)NULL)),
	//							indq(*((IndexQueue*)NULL)){}
	const Service& operator=(const Service &){return *this;}
	
	ObjectVector		&objv;
	IndexStack			&inds;
	Condition			cond;
	Mutex				*mut;
	unsigned			objcnt;//object count
	MutexContainer		mutpool;
};


}//namespace clientserver

#endif
