// proxyservice.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "system/debug.hpp"
#include "utility/dynamicpointer.hpp"

#include "core/manager.hpp"

#include "proxy/proxyservice.hpp"
#include "proxymulticonnection.hpp"

using namespace solid;

namespace concept{
namespace proxy{

Service::Service(Manager &_rm):BaseT(_rm){
}

Service::~Service(){
}

ObjectUidT Service::insertConnection(
	const solid::ResolveData &_rai,
	solid::frame::aio::openssl::Context *_pctx,
	bool _secure
){
	return frame::invalid_uid();
}
/*virtual*/ ObjectUidT Service::insertConnection(
	const solid::SocketDevice &_rsd,
	solid::frame::aio::openssl::Context *_pctx,
	bool _secure
){
	DynamicPointer<frame::aio::Object>	conptr(new MultiConnection(_rsd));
	ObjectUidT rv = this->registerObject(*conptr);
	Manager::the().scheduleAioObject(conptr);
	return rv;
}

}//namespace proxy
}//namespace concept

