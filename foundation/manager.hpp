/* Declarations file manager.hpp
	
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

#ifndef CS_MANAGER_HPP
#define CS_MANAGER_HPP

//#include <vector>

#include "common.hpp"
#include "objectpointer.hpp"

#include "utility/dynamicpointer.hpp"

class  Mutex;
class  SpecificMapper;
class  GlobalMapper;
struct AddrInfoIterator;

namespace foundation{

class	Service;
class	Pool;
class	Object;
class	ActiveSet;
class	Visitor;
class	FileManager;
class	ServiceContainer;
class	Signal;

namespace ipc{
class Service;
}
//! The central class of solidground system
/*!
	<b>Overview:</b><br>
	- Although usually you don't need more than a manager per process,
	the design allows that.
	- The manager keeps the services and should keep the workpools (it 
	means that the foundation::Manager does not keep any workpool, but
	the inheriting manager should).
	- The manager object can be easely accessed from any of the manager's
	thread through thread specific: use Manager::the() method.
	
	<b>Usage:</b><br>
	- Inherit, add workpools and extend.
	
	<b>Notes:</b><br>
*/
class Manager{
public:
	virtual ~Manager();
	//! Easy access to manager using thread specific
	static Manager& the();
	
	//! Signal an object identified by (id,uid) with a sinal mask
	int signalObject(IndexTp _fullid, uint32 _uid, ulong _sigmask);
	
	//! Signal an object with a signal mask, given a reference to the object
	int signalObject(Object &_robj, ulong _sigmask);
	
	//! Signal an object identified by (id,uid) with a signal
	int signalObject(IndexTp _fullid, uint32 _uid, DynamicPointer<Signal> &_rsig);
	
	//! Signal an object with a signal, given a reference to the object
	int signalObject(Object &_robj, DynamicPointer<Signal> &_rsig);
	
	//! Wake an object
	void raiseObject(Object &_robj);
	
	//! Get the mutex associated to the given object
	Mutex& mutex(Object &_robj)const;
	//! Get the unique id associated to the given object
	uint32  uid(Object &_robj)const;
	
	//! Get a reference to the filemanager
	FileManager& fileManager();
	
	//! Remove the file manager - do not use this
	void removeFileManager();
	
	//! Get a reference to the ipc service
	ipc::Service& ipc();
	
	//! Insert a talker into the ipc service
	int insertIpcTalker(
		const AddrInfoIterator &_rai,
		const char*_node = NULL,
		const char *_srv = NULL
	);
	
	//! Unsafe - you should not use this
	template<class T>
	typename T::ServiceTp& service(const T &_robj){
		return static_cast<typename T::ServiceTp&>(service(_robj.serviceid()));
	}
	//! Prepare a manager thread
	/*!
		The method is called (should be called) from every
		manager thread, to initiate thread specific data. In order to extend
		the initialization of specific data, implement the virtual protected method:
		doPrepareThread.
	 */
	void prepareThread();
	//! Unprepare a manager thread
	/*!
		The method is called (should be called) from every
		manager thread, to initiate thread specific data. In order to extend
		the initialization of specific data, implement the virtual protected method:
		doUnprepareThread.
	 */
	void unprepareThread();
	virtual SpecificMapper*  specificMapper();
	virtual GlobalMapper* globalMapper();
	//! Register an activeset / workpool
	uint registerActiveSet(ActiveSet &_ras);
	//! Get the service id for a service
	//uint serviceId(const Service &_rs)const;
	//! Remove a service
	/*!
		The services are also objects so this must be called when
		receiving S_KILL within service's execute method, outside
		service's associated mutex lock.
	*/
	void removeService(Service *_ps);
protected:
	//! Implement this method to exted thread preparation
	virtual void doPrepareThread(){}
	//! Implement this method to exted thread unpreparation
	virtual void doUnprepareThread(){}
	//! Registers a new service.
	/*! 
		- The service MUST be stopped before adding it.
		- All it does is to register the service as an object, into 
			a service of services which will permit signaling capabilities
			for services.
		- You must in case of success, add the service into a ObjectSelector
			select pool.
		\return the id of the service
	*/
	int insertService(Service *_ps);
	//! Add objects that are on the same level as the services but are not services
	void insertObject(Object *_po);
	//! Remove an object
	void removeObject(Object *_po);
	//! Get the number of services
	unsigned serviceCount()const;
	//! Get the service at index _i
	Service& service(uint _i = 0)const;
	//! Constructor with filemanager pointer and ipc service
	Manager(FileManager *_pfm = NULL, ipc::Service *_pipcs = NULL);
	//! Set the filemanager
	void fileManager(FileManager *_pfm);
	//! Set the ipc service
	void ipc(ipc::Service *_pipcs);
	void stop(bool _wait = true);
private:
	ServiceContainer & serviceContainer();
	//Manager(const Manager&){}
	Manager& operator=(const Manager&);
private:
	class ServicePtr;
	struct Data;
	Data &d;
};

}//namespace
#endif
