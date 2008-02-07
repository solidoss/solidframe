/* Implementation file echoservice.cpp
	
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

#include "system/debug.h"
#include "core/server.h"
#include "clientserver/core/objptr.h"
#include "clientserver/udp/station.h"
#include "clientserver/tcp/station.h"
#include "clientserver/tcp/channel.h"

#include "core/listener.h"

#include "echo/echoservice.h"
#include "echoconnection.h"
#include "echotalker.h"

namespace cs = clientserver;

namespace test{
namespace echo{

test::Service* Service::create(){
	return new Service;
}

Service::Service(){
}

Service::~Service(){
}

int Service::insertConnection(
	test::Server &_rs,
	cs::tcp::Channel *_pch
){
	Connection *pcon = new Connection(_pch, 0);
	if(this->insert(*pcon, this->index())){
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
	if(this->insert(*plis, this->index())){
		delete plis;
		return BAD;
	}	
	_rs.pushJob((cs::tcp::Listener*)plis);
	return OK;
}
int Service::insertTalker(
	Server &_rs, 
	const AddrInfoIterator &_rai,
	const char *_node,
	const char *_svc
){
	cs::udp::Station *pst(cs::udp::Station::create(_rai));
	if(!pst) return BAD;
	Talker *ptkr = new Talker(pst, _node, _svc);
	if(this->insert(*ptkr, this->index())){
		delete ptkr;
		return BAD;
	}
	_rs.pushJob((cs::udp::Talker*)ptkr);
	return OK;
}

int Service::insertConnection(
	Server &_rs, 
	const AddrInfoIterator &_rai,
	const char *_node,
	const char *_svc
){
	cs::tcp::Channel *pch(cs::tcp::Channel::create(_rai));
	if(!pch) return BAD;
	Connection *pcon = new Connection(pch, _node, _svc);
	if(this->insert(*pcon, this->index())){
		delete pcon;
		return BAD;
	}
	_rs.pushJob((cs::tcp::Connection*)pcon);
	return OK;
}

int Service::removeTalker(Talker& _rtkr){
	this->remove(_rtkr);
	return OK;
}

int Service::removeConnection(Connection &_rcon){
	//TODO:
	this->remove(_rcon);
	return OK;
}

}//namespace echo
}//namespace test

