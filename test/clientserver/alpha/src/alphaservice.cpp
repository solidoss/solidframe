/* Implementation file alphaservice.cpp
	
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

#include "system/debug.h"
#include "core/server.h"

#include "algorithm/serialization/binary.h"

#include "clientserver/core/objptr.h"
#include "clientserver/tcp/station.h"

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
	Connection *pcon = new Connection(_pch, 0);
	if(this->insert(*pcon, _rs.serviceId(*this))){
		delete pcon;
		return BAD;
	}	
	_rs.pushJob((cs::tcp::Connection*)pcon);
	return OK;
}

int Service::insertListener(
	test::Server &_rs,
	const AddrInfoIterator &_rai
){
	cs::tcp::Station *pst(cs::tcp::Station::create(_rai));
	if(!pst) return BAD;
	test::Listener *plis = new test::Listener(pst, 100, 0);
	if(this->insert(*plis, _rs.serviceId(*this))){
		delete plis;
		return BAD;
	}	
	_rs.pushJob((cs::tcp::Listener*)plis, 0);
	return OK;
}

int Service::removeConnection(Connection &_rcon){
	//TODO:
	this->remove(_rcon);
	return OK;
}

}//namespace alpha
}//namespace test

