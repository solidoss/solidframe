/* Declarations file betaconnection.hpp
	
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

#ifndef BETACONNECTION_H
#define BETACONNECTION_H

#include "core/connection.hpp"
#include "clientserver/core/readwriteobject.hpp"
#include "system/socketaddress.hpp"
class SocketAddress;

namespace clientserver{
class Visitor;
namespace tcp{
class Channel;
}
}

class AddrInfo;

namespace test{
class Visitor;
namespace beta{
class Service;

class Connection: public clientserver::ReadWriteObject<test::Connection>{
public:
	typedef Service	ServiceTp;
	typedef clientserver::ReadWriteObject<test::Connection> BaseTp;
	
	Connection(clientserver::tcp::Channel *_pch, const char *_node = NULL, const char *_srv = NULL);
	~Connection();
	int execute(ulong _sig, TimeSpec &_tout);
	int execute();
	int accept(clientserver::Visitor &);
private:
	enum {BUFSZ = 4*1024};
	enum {INIT,READ, READ_TOUT, WRITE, WRITE_TOUT, CONNECT,CONNECT_TOUT};
	char				bbeg[BUFSZ];
	const char			*bend;
	char				*brpos;
	const char			*bwpos;
	AddrInfo			*pai;
	AddrInfoIterator	it;
};

}//namespace echo
}//namespace test

#endif
