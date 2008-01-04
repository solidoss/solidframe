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
//class StorageManager;

typedef Service* (*ServiceCreator)();
typedef std::map<const char*, ServiceCreator, StrLess>	ServiceMap;

ServiceMap& getServiceMap();
void registerService(ServiceCreator _psc, const char* _pname);

class Server: public clientserver::Server{
public:
	Server();
	~Server();
	static Server& the(){return static_cast<Server&>(clientserver::Server::the());}
	int start(const char *_which = NULL);
	int stop(const char *_which = NULL);
	
	int insertService(const char* _nm, Service* _psrvc);
	int insertCluster(Service *_psrvc);
	void removeService(Service *_psrvc);
	int insertListener(
		const char* _nm,
		const AddrInfoIterator &_rai
	);
	int insertTalker(
		const char* _nm,
		const AddrInfoIterator &_rai,
		const char*_node = NULL,
		const char *_srv = NULL
	);
	
	int insertConnection(
		const char* _nm,
		const AddrInfoIterator &_rai,
		const char*_node,
		const char *_srv
	);
	
	int visitService(const char* _nm, Visitor &_roi);
	
	//StorageManager& storage();
	serialization::bin::RTTIMapper &binMapper();
	template <class J>
	void pushJob(J *_pj, int _pos = 0);
private:
	struct Data;
	Data	&d;
};

}//namespace test
#endif
