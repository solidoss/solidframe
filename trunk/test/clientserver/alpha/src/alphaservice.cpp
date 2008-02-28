/* Implementation file alphaservice.cpp
	
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

#include "system/debug.hpp"
#include "core/server.h"

#include "algorithm/serialization/binary.hpp"

#include "clientserver/core/objptr.hpp"
#include "clientserver/tcp/station.hpp"

#include "core/listener.h"

#include "alpha/alphaservice.h"
#include "alphaconnection.h"

namespace cs=clientserver;

namespace test{
namespace alpha{

struct InitServiceOnce{
	InitServiceOnce(Server &_rs);
};

InitServiceOnce::InitServiceOnce(Server &_rs){
	Connection::initStatic(_rs);
}

test::Service* Service::create(Server &_rsrv){
	static InitServiceOnce	init(_rsrv);
	return new Service();
}

Service::Service(){
}

Service::~Service(){
}

int Service::insertConnection(
	test::Server &_rs,
	clientserver::tcp::Channel *_pch
){
	//create a new connection with the given channel
	Connection *pcon = new Connection(_pch, 0);
	//register it into the service
	if(this->insert(*pcon, this->index())){
		delete pcon;
		return BAD;
	}
	// add it into a connection pool
	_rs.pushJob((cs::tcp::Connection*)pcon);
	return OK;
}

int Service::insertListener(
	test::Server &_rs,
	const AddrInfoIterator &_rai
){
	//first create a station
	cs::tcp::Station *pst(cs::tcp::Station::create(_rai));
	if(!pst) return BAD;
	//then a listener using the created station
	test::Listener *plis = new test::Listener(pst, 100, 0);
	//register the listener into the service
	if(this->insert(*plis, this->index())){
		delete plis;
		return BAD;
	}
	// add the listener into a listener pool
	_rs.pushJob((cs::tcp::Listener*)plis, 0);
	return OK;
}

int Service::removeConnection(Connection &_rcon){
	//TODO:
	//unregisters the connection from the service.
	this->remove(_rcon);
	return OK;
}

}//namespace alpha
}//namespace test

