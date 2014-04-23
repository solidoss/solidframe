// service.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <cstdlib>

#include "system/debug.hpp"
#include "system/timespec.hpp"
#include "system/mutex.hpp"
#include "system/cassert.hpp"

#include "frame/aio/openssl/opensslsocket.hpp"

#include "core/service.hpp"
#include "core/manager.hpp"
#include "utility/dynamicpointer.hpp"

#include "listener.hpp"

using namespace solid;

namespace concept{


Service::Service(Manager &_rm):frame::Service(_rm){
}

Service::~Service(){
}


const char *certificate_path(){
	const char	*path = OSSL_SOURCE_PATH;
	const size_t path_len = strlen(path);
	if(path_len){
		if(path[path_len - 1] == '/'){
			return OSSL_SOURCE_PATH"ssl_/certs/A-server.pem";
		}else{
			return OSSL_SOURCE_PATH"/ssl_/certs/A-server.pem";
		}
	}else return "A-client.pem";
}

bool Service::insertListener(
	const ResolveData &_rai,
	bool _secure
){
	SocketDevice					sd;
	sd.create(_rai.begin());
	sd.makeNonBlocking();
	sd.prepareAccept(_rai.begin(), 100);
	if(!sd.ok()){
		return false;
	}
	
	frame::aio::openssl::Context	*pctx = NULL;
	
	if(_secure){
		pctx = frame::aio::openssl::Context::create();
	}
	
	if(pctx){
		const char *pcertpath = certificate_path();
		
		pctx->loadCertificateFile(pcertpath);
		pctx->loadPrivateKeyFile(pcertpath);
	}
	
	solid::DynamicPointer<solid::frame::aio::Object> lisptr(new Listener(*this, sd, pctx));
	
	this->registerObject(*lisptr);
	
	Manager::the().scheduleListener(lisptr);
	return true;
}
/*virtual*/ ObjectUidT Service::insertConnection(
	const solid::SocketDevice &_rsd,
	solid::frame::aio::openssl::Context *_pctx,
	bool _secure
){
	return frame::invalid_uid();
}

}//namespace concept
