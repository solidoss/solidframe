/* Declarations file betaservice.h
	
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

#ifndef BETASERVICE_H
#define BETASERVICE_H

#include "core/service.h"

namespace clientserver{

namespace tcp{
class Channel;
class Station;
}

namespace udp{
class Station;
class Talker;
}

}//namespace clientserver

namespace test{
namespace beta{

class Connection;
class Talker;

class Service: public test::Service{
public:
	static test::Service* create();
	Service();
	~Service();
	int insertConnection(
		test::Server &_rs,
		clientserver::tcp::Channel *_pch
	);
	int insertListener(
		test::Server &_rs,
		const AddrInfoIterator &_rai
	);
	int insertTalker(
		Server &_rs, 
		const AddrInfoIterator &_rai,
		const char *_node,
		const char *_svc
	);
	int insertConnection(
		Server &_rs, 
		const AddrInfoIterator &_rai,
		const char *_node,
		const char *_svc
	);
	int removeConnection(Connection &);
	int removeTalker(Talker&);
};

}//namespace echo
}//namespace test
#endif
