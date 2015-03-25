// frame/aio/openssl/src/aiocompletion.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/aio/openssl/aiosecuresocket.hpp"
#include "frame/aio/openssl/aiosecurecontext.hpp"
#include "system/mutex.hpp"

namespace solid{
namespace frame{
namespace aio{
namespace openssl{


struct Starter{
	Mutex	*pmtxes;
	
	Starter(){
		
	}
	~Starter(){
		
	}
};

//=============================================================================

/*static*/ Context Context::create(const SSL_METHOD* _pm/*= NULL*/){
	static Starter starter;
	Context	rv;
	if(_pm == NULL){
		rv.pctx = ::SSL_CTX_new(::SSLv23_method());
	}else{
		rv.pctx = ::SSL_CTX_new(_pm);
	}
	return rv;
}

bool Context::ok()const{
	return pctx != NULL;
}

Context::Context(Context && _rctx){
	pctx = _rctx.pctx;
	_rctx.pctx = NULL;
}
Context& Context::operator=(Context && _rctx){
	if(ok()){
		SSL_CTX_free(pctx);
		pctx = NULL;
	}
	pctx = _rctx.pctx;
	_rctx.pctx = NULL;
}
Context::Context():pctx(NULL){
	
}
Context::~Context(){
	if(ok()){
		SSL_CTX_free(pctx);
		pctx = NULL;
	}
}


ErrorCodeT Context::loadFile(const char *_path){
	if(SSL_CTX_load_verify_locations(pctx, _path, NULL)){
		return ErrorCodeT(-1, ErrorCodeT::category());
	}else{
		return ErrorCodeT();
	}
}
ErrorCodeT Context::loadPath(const char *_path){
	if(SSL_CTX_load_verify_locations(pctx, NULL, _path)){
		return ErrorCodeT(-1, ErrorCodeT::category());
	}else{
		return ErrorCodeT();
	}
}

ErrorCodeT Context::loadCertificateFile(const char *_path){
	if(SSL_CTX_use_certificate_file(pctx, _path, SSL_FILETYPE_PEM)){
		return ErrorCodeT(-1, ErrorCodeT::category());
	}else{
		return ErrorCodeT();
	}
}

ErrorCodeT Context::loadPrivateKeyFile(const char *_path){
	if(SSL_CTX_use_PrivateKey_file(pctx, _path, SSL_FILETYPE_PEM)){
		return ErrorCodeT(-1, ErrorCodeT::category());
	}else{
		return ErrorCodeT();
	}
}

//=============================================================================


Socket::Socket(Context &_rctx, SocketDevice &_rsd){}
	
Socket::Socket(){}

Socket::~Socket(){}

SocketDevice Socket::reset(Context &_rctx, SocketDevice &_rsd){}

void Socket::shutdown(){
	
}

bool Socket::create(Context &_rctx, SocketAddressStub const &_rsas){}

bool Socket::connect(SocketAddressStub const &_rsas, bool &_can_retry){}

ReactorEventsE filterReactorEvents(
	const  ReactorEventsE _evt,
	const bool /*_pending_recv*/,
	const bool /*_pendign_send*/
) const{
	
}

int Socket::recv(char *_pb, size_t _bl, bool &_can_retry){}

int Socket::send(const char *_pb, size_t _bl, bool &_can_retry){}

SocketDevice const& Socket::device()const{
	return sd;
}

int Socket::recvFrom(char *_pb, size_t _bl, SocketAddress &_addr, bool &_can_retry){}

int Socket::sendTo(const char *_pb, size_t _bl, SocketAddressStub const &_rsas, bool &_can_retry){}

bool Socket::secureAccept(bool &_can_retry){
	
}

bool Socket::secureConnect(bool &_can_retry){

}

bool Socket::secureShutdown(bool &_can_retry){
	
}

}//namespace openssl
}//namespace aio
}//namespace frame
}//namespace solid



