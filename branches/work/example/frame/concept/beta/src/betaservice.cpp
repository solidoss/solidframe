// betaservice.cpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
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

