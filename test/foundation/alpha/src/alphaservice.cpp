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
#include "core/manager.hpp"


#include "algorithm/serialization/binary.hpp"

#include "foundation/objectpointer.hpp"
#include "foundation/aio/openssl/opensslsocket.hpp"

#include "core/listener.hpp"

#include "alpha/alphaservice.hpp"
#include "alphaconnection.hpp"

namespace fdt=foundation;

namespace test{
namespace alpha{

struct InitServiceOnce{
	InitServiceOnce(Manager &_rm);
};

InitServiceOnce::InitServiceOnce(Manager &_rm){
	Connection::initStatic(_rm);
}

test::Service* Service::create(Manager &_rm){
	static InitServiceOnce	init(_rm);
	return new Service();
}

Service::Service(){
}

Service::~Service(){
}

int Service::insertConnection(
	test::Manager &_rm,
	const SocketDevice &_rsd,
	foundation::aio::openssl::Context *_pctx,
	bool _secure
){
	//create a new connection with the given channel
	Connection *pcon = new Connection(_rsd);
	if(_pctx){
		pcon->socketSecureSocket(_pctx->createSocket());
	}
	//register it into the service
	if(this->insert(*pcon, this->index())){
		delete pcon;
		return BAD;
	}
	// add it into a connection pool
	_rm.pushJob(static_cast<fdt::aio::Object*>(pcon));
	return OK;
}

int Service::insertListener(
	test::Manager &_rm,
	const AddrInfoIterator &_rai,
	bool _secure
){
	SocketDevice sd;
	sd.create(_rai);
	sd.makeNonBlocking();
	sd.prepareAccept(_rai, 100);
	if(!sd.ok()) return BAD;
	
	foundation::aio::openssl::Context *pctx = NULL;
	if(_secure){
		pctx = foundation::aio::openssl::Context::create();
	}
	if(pctx){
		pctx->loadCertificateFile("../../../../../extern/linux/openssl/demos/tunala/A-server.pem");
		pctx->loadPrivateKeyFile("../../../../../extern/linux/openssl/demos/tunala/A-server.pem");
	}
	test::Listener *plis = new test::Listener(sd, pctx);
	
	if(this->insert(*plis, this->index())){
		delete plis;
		return BAD;
	}	
	_rm.pushJob(static_cast<fdt::aio::Object*>(plis));
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

