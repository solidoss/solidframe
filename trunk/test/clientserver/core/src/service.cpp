/* Implementation file service.cpp
	
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

#include <cstdlib>

#include "system/debug.hpp"
#include "system/timespec.hpp"

#include "core/service.h"
#include "core/server.h"
#include "core/listener.h"
#include "clientserver/tcp/station.hpp"
#include "clientserver/tcp/channel.hpp"
#include "clientserver/udp/station.hpp"

namespace cs = clientserver;

namespace test{
/*
	A service is also an object and it can do something.
	Here's what it does by default.
	
*/
int Service::execute(ulong _sig, TimeSpec &_rtout){
	idbg("serviceexec");
	if(signaled()){
		ulong sm;
		{
			Mutex::Locker	lock(*mut);
			sm = grabSignalMask(1);
		}
		if(sm & cs::S_KILL){
			idbg("killing service "<<this->id());
			this->stop(test::Server::the(), true);
			test::Server::the().removeService(this);
			return BAD;
		}
	}
	return NOK;
}

Service::~Service(){
}

int Service::removeListener(Listener &_rlis){
	this->remove(_rlis);
	return OK;
}

// Some dummy insert methods

int Service::insertListener(
	Server &_rsrv,
	const AddrInfoIterator &_rai
){
	return BAD;
}
int Service::insertTalker(
	Server &_rs, 
	const AddrInfoIterator &_rai,
	const char *_node,
	const char *_svc
){
	
	return BAD;
}
int Service::insertConnection(
	Server &_rs,
	const AddrInfoIterator &_rai,
	const char *_node,
	const char *_svc
){	
	return BAD;
}
int Service::insertConnection(
	Server &_rs, 
	cs::tcp::Channel *_pch
){	
	cassert(false);
	delete _pch;
	return BAD;
}

}
