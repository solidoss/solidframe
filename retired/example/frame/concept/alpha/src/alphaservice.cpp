// alphaservice.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
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

#include "alpha/alphaservice.hpp"
#include "alphaconnection.hpp"
#include "alphasteward.hpp"

using namespace solid;

namespace concept{
namespace alpha{

typedef DynamicSharedPointer<Steward>	StewardSharedPointerT;
struct Service::Data{
	StewardSharedPointerT	stewardptr;
};


#ifdef HAS_SAFE_STATIC

struct InitServiceOnce{
	InitServiceOnce(Manager &_rm);
};

InitServiceOnce::InitServiceOnce(Manager &_rm){
	Connection::initStatic(_rm);
}

Service::Service(Manager &_rm):BaseT(_rm), d(*(new Data)){
	static InitServiceOnce once(_rm);
	d.stewardptr = new Steward(_rm);
	_rm.registerObject(*d.stewardptr);

	solid::DynamicPointer<solid::frame::Object>	objptr(d.stewardptr);
	_rm.scheduleObject(objptr);
}

#else
void once_init(){
	Connection::initStatic(_rm);
}

Service::Service(Manager &_rm):BaseT(_rm){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_init, once);
}

#endif

Service::~Service(){
	delete &d;
}


ObjectUidT Service::insertConnection(
	solid::ResolveData &_rai,
	solid::frame::aio::openssl::Context *_pctx,
	bool _secure
){
	DynamicPointer<frame::aio::Object>	conptr(new Connection(_rai));
	ObjectUidT rv = this->registerObject(*conptr);
	Manager::the().scheduleAioObject(conptr);
	return rv;
}
/*virtual*/ ObjectUidT Service::insertConnection(
	const solid::SocketDevice &_rsd,
	solid::frame::aio::openssl::Context *_pctx,
	bool _secure
){
	DynamicPointer<frame::aio::Object>	conptr(new Connection(_rsd));
	ObjectUidT rv = this->registerObject(*conptr);
	Manager::the().scheduleAioObject(conptr);
	return rv;
}

}//namespace alpha
}//namespace concept

