/* Implementation file service.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstdlib>

#include "system/debug.hpp"
#include "system/timespec.hpp"
#include "system/mutex.hpp"
#include "system/cassert.hpp"

#include "foundation/aio/openssl/opensslsocket.hpp"

#include "core/service.hpp"
#include "core/manager.hpp"
#include "core/listener.hpp"

namespace fdt = foundation;

namespace concept{


/*static*/ void Service::dynamicRegister(){
	BaseT::dynamicRegister();
	DynamicExecuterT::registerDynamic<SocketAddressInfoSignal, Service>();
}

static const DynamicRegisterer<Service>	dre;

Service::Service(){
	registerObjectType<Listener>(this);
}

Service::~Service(){
}

void Service::dynamicExecute(DynamicPointer<SocketAddressInfoSignal> &_rsig){
	idbg(_rsig->id);
	int rv;
	switch(_rsig->id){
		case AddListener:
			rv = this->insertListener(_rsig->addrinfo.begin(), false);
			_rsig->result(rv);
			break;
		case AddSslListener:
			rv = this->insertListener(_rsig->addrinfo.begin(), true);
			_rsig->result(rv);
			break;
		case AddConnection:
			rv = this->insertConnection(_rsig->addrinfo.begin(), _rsig->node.c_str(), _rsig->service.c_str());
			_rsig->result(rv);
			break;
		case AddSslConnection:
			cassert(false);
			//TODO:
			break;
		case AddTalker:
			rv = this->insertTalker(_rsig->addrinfo.begin(), _rsig->node.c_str(), _rsig->service.c_str());
			_rsig->result(rv);
			break;
		default:
			cassert(false);
	}
}

void Service::insertObject(Listener &_ro, const ObjectUidT &_ruid){
	
}

void Service::eraseObject(const Listener &_ro){
	
}

bool Service::insertListener(
	const SocketAddressInfoIterator &_rai,
	bool _secure
){
	SocketDevice sd;
	sd.create(_rai);
	sd.makeNonBlocking();
	sd.prepareAccept(_rai, 100);
	if(!sd.ok()){
		return false;
	}
	
	foundation::aio::openssl::Context *pctx = NULL;
	if(_secure){
		pctx = foundation::aio::openssl::Context::create();
	}
	if(pctx){
		const char *pcertpath(OSSL_SOURCE_PATH"ssl_/certs/A-server.pem");
		
		pctx->loadCertificateFile(pcertpath);
		pctx->loadPrivateKeyFile(pcertpath);
	}
	
	fdt::ObjectPointer<Listener> lisptr(new Listener(sd, pctx));
	
	insert<AioSchedulerT>(lisptr, 1);
		
	return true;
}


bool Service::insertTalker(
	const SocketAddressInfoIterator &_rai,
	const char *_node,
	const char *_svc
){
	
	return BAD;
}

bool Service::insertConnection(
	const SocketAddressInfoIterator &_rai,
	const char *_node,
	const char *_svc
){	
	return BAD;
}

bool Service::insertConnection(
	const SocketDevice &_rsd,
	foundation::aio::openssl::Context *_pctx,
	bool _secure
){	
	cassert(false);
	return BAD;
}



}//namespace alpha
