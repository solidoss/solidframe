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

#include "frame/aio/openssl/opensslsocket.hpp"

#include "core/service.hpp"
#include "core/manager.hpp"
#include "utility/dynamicpointer.hpp"

#include "listener.hpp"

using namespace solid;

namespace concept{


Service::Service():frame::Service(Manager::the()){
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

void Service::insertListener(
	const ResolveData &_rai,
	bool _secure
){
	SocketDevice					sd;
	sd.create(_rai.begin());
	sd.makeNonBlocking();
	sd.prepareAccept(_rai.begin(), 100);
	if(!sd.ok()){
		return;
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
}

}//namespace concept
