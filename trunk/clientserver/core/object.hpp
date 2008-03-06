/* Declarations file object.hpp
	
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

#ifndef CS_OBJECT_HPP
#define CS_OBJECT_HPP

#include "common.hpp"
#include "cmdptr.hpp"
//#define SERVICEBITCNT	3
//#define SERVICEBITDPL	((sizeof(ulong) * 8) - SERVICEBITCNT)
//#define SERVICEBITMSK	(1+2+4)
//#define MASKBITCNT		3
//#define MASKBITDPL		((sizeof(ulong) * 8) - SERVICEBITCNT - MASKBITCNT)
//#define MASKBITMSK		(1+2+4)
//#define INDEXMSK		(~((SERVICEBITMSK << SERVICEBITDPL) | (MASKBITMSK << MASKBITDPL)))

class Mutex;
struct TimeSpec;
namespace clientserver{

class Visitor;
class Server;
class Service;
class ObjPtrBase;
class Command;
//! A pseudo-active object class
/*!
	<b>Overview:</b><br>
	A pseudo-active object is one that altough it doesnt own a thread
	it can reside within a workpool an receive processor/thread time
	due to events.
	
	So an object:
	- can receive signals
	- can receive commands
	- must reside both on a static container (clientserver::Service) and 
	an ActiveSet (usually clientserver::SelectPool)
	- has an associated mutex which can be requested from the service
	- can be visited using a visitor<br>
	
	<b>Usage:</b><br>
	- Inherit the object and implement execute, adding code for grabing
	the signal mask and the commands under lock.
	- Add code so that the object gets registered on service and inserted
	into a proper workpool upon its creation.
	
	<b>Notes:</b><br>
	- Every object have an associated unique id (a pair of uint32) wich 
	uniquely identifies the object within the server both on memory and on
	time. The solidground architecture and the ones built upon it MUST NOT allow
	access to object pointers, instead they do/should do permit limited access
	using the unique id: sigaling and sending commands through the server interface.
	- Also an object will hold information about the thread in which it is currently
	executed, so that the signaling is fast.
*/
class Object{
public:
	typedef Command	CommandTp;
	enum Defs{
		SERVICEBITCNT = 8,
		INDEXBITCNT	= 32 - SERVICEBITCNT,
		INDEXMASK = 0xffffffff >> SERVICEBITCNT
	};
	//! Extracts the object index within service from an objectid
	static ulong computeIndex(ulong _fullid);
	//! Extracts the service id from an objectid
	static ulong computeServiceId(ulong _fullid);
	//! Constructor
	Object(ulong _fullid = 0);
	
	//getters:
	//! Get the id of the parent service
	uint serviceid()const;
	//! Get the index of the object within service from an objectid
	ulong index()const;

	//! Get the id of the object
	ulong id()			const 	{return fullid;}
	
	//! Gets the id of the thread the object resides in
	void getThread(uint &_rthid, uint &_rthpos);
	//! Returns true if the object is signaled
	uint signaled()			const 	{return smask;}
	
	uint signaled(uint _s)	const;
	
	//setters:
	//! Set the id
	void id(ulong _fullid);
	//! Set the id given the service id and index
	void id(ulong _srvid, ulong _ind);
	//! Set the thread id
	void setThread(uint _thrid, uint _thrpos);
	
	//! Returns the state of the objec -a negative state means the object must be destroyed
	int state()	const 	{return crtstate;}
	/**
	 * Returns true if the signal should raise the object ASAP
	 * \param _smask The signal bitmask
	 */
	int signal(uint _smask);
	//! Signal the object with a command
	virtual int signal(CmdPtr<Command> &_cmd);
	
	//! Executes the objects
	/*!
		This method is calle by basic objectpools with no support for
		events and timeouts
	*/
	virtual int execute();
	//! Executes the objects
	/*!
		This method is calle by selectpools with support for
		events and timeouts
	*/
	virtual int execute(ulong _evs, TimeSpec &_rtout);
	//! Acceptor method for different visitors
	virtual int accept(Visitor &_roi);
	//! This method will be called once by service when registering an object
	/*!
		Some objects may need faster access to their associated mutex, so they
		might want to keep a pointe to it.
	*/
	virtual void mutex(Mutex *_pmut);
protected:
	//! Sets the current state.
	/*! If your object will implement a state machine (and in an asynchronous
	environments it most certanly will) use this base state setting it to
	somethin negative on destruction
	*/
	void state(int _st);
	//! Grab the signal mask eventually leaving some bits set- CALL this inside lock!!
	ulong grabSignalMask(ulong _leave = 0);
	friend class Service;
	friend class ObjPtrBase;
	//! Virtual destructor
	virtual ~Object();//only objptr base can destroy an object
private:
	ulong			fullid;
	uint			smask;
	volatile uint	thrid;//the current thread which (may) execute(s) the object
	volatile uint	thrpos;//
	short			usecnt;//
	short			crtstate;// < 0 -> must die
};
#ifdef UINLINES
#include "src/object.ipp"
#endif
}//namespace

#endif
