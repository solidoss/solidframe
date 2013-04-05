/* Implementation file betaservice.cpp
	
	Copyright 2012 Valentin Palade 
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

#include "system/debug.hpp"
#include "core/manager.hpp"


#include "serialization/binary.hpp"

#include "frame/aio/openssl/opensslsocket.hpp"

#include "beta/betaservice.hpp"
#include "betaclientconnection.hpp"
#include "betaserverconnection.hpp"

using namespace solid;

namespace concept{
namespace beta{

#ifdef HAS_SAFE_STATIC

struct InitServiceOnce{
	InitServiceOnce(Manager &_rm);
};

InitServiceOnce::InitServiceOnce(Manager &_rm){
	client::Connection::initStatic(_rm);
	server::Connection::initStatic(_rm);
}
Service::Service(Manager &_rm):BaseT(_rm){
	static InitServiceOnce	init(_rm);
}
#else
void once_init(){
	client::Connection::initStatic(_rm);
	server::Connection::initStatic(_rm);
}
Service* Service::create(Manager &_rm){
	return new Service();
}
Service::Service(Manager &_rm):BaseT(_rm){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_init, once);
}

#endif


Service::~Service(){
}

ObjectUidT Service::insertConnection(
	const SocketDevice &_rsd,
	frame::aio::openssl::Context *_pctx,
	bool _secure
){
	//create a new connection with the given channel
	DynamicPointer<server::Connection> conptr(new server::Connection(_rsd));
	if(_pctx){
		conptr->socketSecureSocket(_pctx->createSocket());
	}
	ObjectUidT rv = this->registerObject(*conptr);
	
	DynamicPointer<frame::aio::Object> objptr(conptr);
	Manager::the().scheduleAioObject(objptr);
	return rv;
}

ObjectUidT Service::insertConnection(
	const ResolveData&_rai,
	frame::aio::openssl::Context *_pctx,
	bool _secure
){
	DynamicPointer<client::Connection> conptr(new client::Connection(_rai));
	if(_pctx){
		conptr->socketSecureSocket(_pctx->createSocket());
	}
	ObjectUidT rv = this->registerObject(*conptr);
	
	DynamicPointer<frame::aio::Object> objptr(conptr);
	Manager::the().scheduleAioObject(objptr);
	return rv;
}

}//namespace beta
}//namespace concept

