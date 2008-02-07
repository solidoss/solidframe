/* Declarations file server.h
	
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

#ifndef TESTSERVER_H
#define TESTSERVER_H
#include <map>
#include "clientserver/core/server.h"
#include "common.h"

namespace serialization{
namespace bin{
class RTTIMapper;
}
}

namespace test{

class Service;
class Visitor;
class CommandExecuter;

typedef Service* (*ServiceCreator)();
typedef std::map<const char*, ServiceCreator, StrLess>	ServiceMap;

ServiceMap& getServiceMap();
void registerService(ServiceCreator _psc, const char* _pname);
//! A proof of concept test server
/*!
	This is a proof of concept server and should be taken accordingly.
	
	It tries to give an ideea of how the clientserver::Server should be used.
	
	To complicate things a little, it allso keeps a map service name to service,
	so that services can be more naturally accessed by their names.
*/
class Server: public clientserver::Server{
public:
	Server();
	~Server();
	//! Overwrite the clientserver::Server::the to give access to the extended interface
	static Server& the(){return static_cast<Server&>(clientserver::Server::the());}
	//! Starts a specific service or all services
	/*!
		\param _which If not null it represents the name of the service
	*/
	int start(const char *_which = NULL);
	//! Stops a specific service or all services
	/*!
		\param _which If not null it represents the name of the service
	*/
	int stop(const char *_which = NULL);
	
	//! Registers a service given its name and a pointer to a service.
	int insertService(const char* _nm, Service* _psrvc);
	//! Get the id of the command executer specialized for reading
	void readCommandExecuterUid(ObjectUidTp &_ruid);
	//! Get the id of the command executer specialized for writing
	void writeCommandExecuterUid(ObjectUidTp &_ruid);
	//! Removes a service
	void removeService(Service *_psrvc);
	//! Inserts a listener into a service
	/*!
		\param _nm The name of the service
		\param _rai The address the listener should listen on
	*/
	int insertListener(
		const char* _nm,
		const AddrInfoIterator &_rai
	);
	//! Insert a talker into a service
	/*!
		\param _nm The name of the service
		\param _rai The localaddress the talker will bind
		\param _node The destination address name (for use with AddrInfo)
		\param _srv The destination address port (for use with AddrInfo)
	*/
	int insertTalker(
		const char* _nm,
		const AddrInfoIterator &_rai,
		const char*_node = NULL,
		const char *_srv = NULL
	);
	//! Insert a connection into a service
	/*!
		\param _nm The name of the service
		\param _rai The localaddress the connection will bind
		\param _node The destination address name (for use with AddrInfo)
		\param _srv The destination address port (for use with AddrInfo)
	*/
	int insertConnection(
		const char* _nm,
		const AddrInfoIterator &_rai,
		const char*_node,
		const char *_srv
	);
	//! Visit all services
	int visitService(const char* _nm, Visitor &_roi);
	
	//! The servers binary mapper for used in binary serializations
	/*!
		Anyone who wants to serialize something must register its structures
		to binMapper
	*/
	serialization::bin::RTTIMapper &binMapper();
	//! Pushes an object into a specific pool.
	template <class J>
	void pushJob(J *_pj, int _pos = 0);
private:
	friend class CommandExecuter;
// 	void removeCommandExecuter(ReadCommandExecuter *);
// 	void removeCommandExecuter(WriteCommandExecuter *);
	struct Data;
	Data	&d;
};

}//namespace test
#endif
