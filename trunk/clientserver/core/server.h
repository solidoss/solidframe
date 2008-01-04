/* Declarations file server.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CS_SERVER_H
#define CS_SERVER_H

//#include <vector>

#include "common.h"
#include "objptr.h"
#include "cmdptr.h"

class  Mutex;
class  SpecificMapper;
class  GlobalMapper;
struct AddrInfoIterator;

namespace clientserver{
class	Service;
class	Pool;
class	Object;
class	ActiveSet;
class	Visitor;
class	StorageManager;

namespace ipc{
class Service;
}

class Server{
public:
	virtual ~Server();
	static Server& the();
	//void removeObject(Object *_pobj, uint _poolid);
	
	int signalObject(ulong _fullid, ulong _uid, ulong _sigmask);
		
	int signalObject(Object &_robj, ulong _sigmask);
	
	int signalObject(ulong _fullid, ulong _uid, CmdPtr<Command> &_cmd);
	
	int signalObject(Object &_robj, CmdPtr<Command> &_cmd);
	
	
	void raiseObject(Object &_robj);
	
	Mutex& mutex(Object &_robj)const;
	ulong  uid(Object &_robj)const;
	
	StorageManager& storage(){return *psm;}
	void removeStorage();
	ipc::Service& ipc(){return *pipcs;}
	
	int insertIpcTalker(
		const AddrInfoIterator &_rai,
		const char*_node = NULL,
		const char *_srv = NULL
	);
	
	template<class T>
	typename T::ServiceTp& service(const T &_robj){
		return static_cast<typename T::ServiceTp&>(service(_robj.serviceid()));
	}
	/**
	 * the folowing two methods are called (should be called) from every
	 * server thread, to initiate thread specific data. In order to extend
	 * the initialization of specific data, implement the virtual protected methods:
	 * doPrepareThread and doUnprepareThread.
	 */
	void prepareThread();
	void unprepareThread();
	virtual SpecificMapper*  specificMapper();
	virtual GlobalMapper* globalMapper();
	uint registerActiveSet(ActiveSet &_ras);
	uint serviceId(const Service &_rs)const;
	void removeService(Service *_ps);
protected:
	virtual void doPrepareThread(){}
	virtual void doUnprepareThread(){}
	/** Adds a new special service.
	 * NOTE: The service MUST be stopped before adding it.
	 * NOTE: A special service is one that need to have a design time id.
	 * \return the id of the service
	 */
	int insertService(Service *_ps);
	/** Add objects that are on the same level as the services but are not services
	 * 
	 */
	void insertObject(Object *_po);
	void removeObject(Object *_po);
	unsigned serviceCount()const;
	Service& service(uint _i = 0)const;
	Server(StorageManager *_psm = NULL, ipc::Service *_pipcs = NULL);
	void storage(StorageManager *_psm);
	void ipc(ipc::Service *_pipcs);
private:
	//Server(const Server&){}
	Server& operator=(const Server&){return *this;}
	class ServicePtr;
	class ServiceVector;
	class ActiveSetVector;
	
	ServiceVector			&servicev;
	ActiveSetVector			&asv;
	ObjPtr<StorageManager>	psm;
	ipc::Service			*pipcs;
};

}//namespace
#endif
