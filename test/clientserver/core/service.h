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

#ifndef TESTSERVICE_H
#define TESTSERVICE_H

#include "clientserver/core/readwriteservice.h"
#include "common.h"

struct AddrInfoIterator;

namespace clientserver{
namespace tcp{
class Station;
class Channel;
}

namespace udp{
class Station;
}

}


namespace test{

class Server;
class Visitor;

class Listener;
class Talker;

class Service: public clientserver::ReadWriteService{
public:
	Service(){}
	~Service();
	virtual int execute(ulong _evs, TimeSpec &_rtout);
	virtual int insertListener(
		Server &_rsrv,
		const AddrInfoIterator &_rai
	);
	virtual int insertTalker(
		Server &_rs,
		const AddrInfoIterator &_rai,
		const char *_node,
		const char *_svc
	);
	//this is used by the generic listener
	virtual int insertConnection(
		Server &_rs,
		clientserver::tcp::Channel *_pch
	);
	virtual int insertConnection(
		Server &_rs,
		const AddrInfoIterator &_rai,
		const char *_node,
		const char *_svc
	);
	virtual int removeListener(Listener &);
	
};

}//namespace test
#endif
