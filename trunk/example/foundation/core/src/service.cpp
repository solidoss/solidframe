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
#include "system/mutex.hpp"
#include "system/cassert.hpp"

#include "foundation/aio/openssl/opensslsocket.hpp"

#include "core/service.hpp"
#include "core/manager.hpp"
#include "core/listener.hpp"

namespace fdt = foundation;

namespace concept{


/*static*/ void Service::dynamicRegister(){
	DynamicReceiverT::add<AddrInfoSignal, Service>();
}

static const DynamicRegisterer<Service>	dre;


int Service::insertTalker(
	const AddrInfoIterator &_rai,
	const char *_node,
	const char *_svc
){
	
	return BAD;
}

int Service::insertConnection(
	const AddrInfoIterator &_rai,
	const char *_node,
	const char *_svc
){	
	return BAD;
}

int Service::insertConnection(
	const SocketDevice &_rsd,
	foundation::aio::openssl::Context *_pctx,
	bool _secure
){	
	cassert(false);
	return BAD;
}

int Service::dynamicExecuteDefault(DynamicPointer<> &_dp){
	wdbg("Received unknown signal on ipcservice");
	return BAD;
}

int Service::dynamicExecute(DynamicPointer<AddrInfoSignal> &_rsig){
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
	return rv;
}


/*virtual*/ int Service::signal(DynamicPointer<foundation::Signal> &_sig){
	if(this->state() < 0){
		_sig.clear();
		return 0;//no reason to raise the pool thread!!
	}
	dr.push(DynamicPointer<>(_sig));
	return Object::signal(fdt::S_SIG | fdt::S_RAISE);
}

/*
	A service is also an object and it can do something.
	Here's what it does by default.
	
*/
int Service::execute(ulong _sig, TimeSpec &_rtout){
	idbg("serviceexec");
	if(signaled()){
		ulong sm;
		{
			Mutex::Locker	lock(*mutex());
			sm = grabSignalMask(1);
			if(sm & fdt::S_SIG){//we have signals
				dr.prepareExecute();
			}
		}
		if(sm & fdt::S_SIG){//we've grabed signals, execute them
			while(dr.hasCurrent()){
				dr.executeCurrent(*this);
				dr.next();
			}
		}
		if(sm & fdt::S_KILL){
			idbgx(Dbg::ipc, "killing service "<<this->id());
			this->stop(true);
			Manager::the().removeService(static_cast<concept::Service*>(this));
			return BAD;
		}
	}
	return NOK;
}

Service::~Service(){
}

void Service::removeListener(Listener &_rlis){
	this->remove(_rlis);
}


int Service::insertListener(
	const AddrInfoIterator &_rai,
	bool _secure
){
	SocketDevice sd;
	sd.create(_rai);
	sd.makeNonBlocking();
	sd.prepareAccept(_rai, 100);
	if(!sd.ok())
		return BAD;
	
	foundation::aio::openssl::Context *pctx = NULL;
	if(_secure){
		pctx = foundation::aio::openssl::Context::create();
	}
	if(pctx){
		const char *pcertpath = OSSL_SOURCE_PATH"ssl_/certs/A-server.pem";
		pctx->loadCertificateFile(pcertpath);
		pctx->loadPrivateKeyFile(pcertpath);
	}
	
	Listener *plis = new Listener(sd, pctx);
	
	if(this->insert(*plis, this->index())){
		delete plis;
		return BAD;
	}	
	Manager::the().pushJob(static_cast<fdt::aio::Object*>(plis));
	return OK;
}

}//namespace alpha
