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
#include "system/thread.hpp"

#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/conf.h"

namespace solid{
namespace frame{
namespace aio{
namespace openssl{


struct Starter{
	Mutex	*pmtxes;
	
	 static void ssl_locking_fnc(int mode, int n, const char* /*file*/, int /*line*/){
		if (mode & CRYPTO_LOCK)
			the().pmtxes[n].lock();
		else
			the().pmtxes[n].unlock();
	}
	
	static unsigned long ssl_threadid_fnc()
	{
		return Thread::currentId();
	}
	
	Starter():pmtxes(nullptr){
		pmtxes = new Mutex[::CRYPTO_num_locks()];
		::SSL_library_init();
		::SSL_load_error_strings();
		::OpenSSL_add_all_algorithms();

		::CRYPTO_set_locking_callback(&Starter::ssl_locking_fnc);
		::CRYPTO_set_id_callback(&Starter::ssl_threadid_fnc);
	}
	~Starter(){
		::CRYPTO_set_id_callback(0);
		::CRYPTO_set_locking_callback(0);
		::ERR_free_strings();
		::ERR_remove_state(0);
		::EVP_cleanup();
		::CRYPTO_cleanup_all_ex_data();
		::CONF_modules_unload(1);
		
		
		delete []pmtxes;
	}
	
	static Starter& the(){
		static Starter s;
		return s;
	}
	
};

//=============================================================================

/*static*/ Context Context::create(const SSL_METHOD* _pm/*= NULL*/){
	Starter::the();
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
	return *this;
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
	ErrorCodeT err;
	if(SSL_CTX_load_verify_locations(pctx, _path, NULL)){
		err.assign(-1, err.category());
	}
	return err;
}
ErrorCodeT Context::loadPath(const char *_path){
	ErrorCodeT err;
	if(SSL_CTX_load_verify_locations(pctx, NULL, _path)){
		err.assign(-1, err.category());
	}
	return err;
}

ErrorCodeT Context::loadCertificateFile(const char *_path){
	ErrorCodeT err;
	if(SSL_CTX_use_certificate_file(pctx, _path, SSL_FILETYPE_PEM)){
		err.assign(-1, err.category());
	}
	return err;
}

ErrorCodeT Context::loadPrivateKeyFile(const char *_path){
	ErrorCodeT err;
	if(SSL_CTX_use_PrivateKey_file(pctx, _path, SSL_FILETYPE_PEM)){
		err.assign(-1, err.category());
	}
	return err;
}

//=============================================================================


Socket::Socket(Context &_rctx, SocketDevice &_rsd):sd(_rsd){
	pssl = SSL_new(_rctx.pctx);
	
}
	
Socket::Socket(Context &_rctx){
	pssl = SSL_new(_rctx.pctx);
}

Socket::~Socket(){
	SSL_free(pssl);
}

SocketDevice Socket::reset(Context &_rctx, SocketDevice &_rsd, ErrorCodeT &_rerr){
	return SocketDevice();
}

void Socket::shutdown(){
	sd.shutdownReadWrite();
}

bool Socket::create(SocketAddressStub const &_rsas, ErrorCodeT &_rerr){
	_rerr = sd.create(_rsas.family());
	if(!_rerr){
		_rerr = sd.makeNonBlocking();
	}
	return !_rerr;
}

bool Socket::connect(SocketAddressStub const &_rsas, bool &_can_retry, ErrorCodeT &_rerr){
	_rerr = sd.connect(_rsas, _can_retry);
	return !_rerr;
}

ReactorEventsE Socket::filterReactorEvents(
	const  ReactorEventsE _evt,
	const bool /*_pending_recv*/,
	const bool /*_pendign_send*/
) const{
	return _evt;
}

int Socket::recv(char *_pb, size_t _bl, bool &_can_retry, ErrorCodeT &_rerr){
	return -1;
}

int Socket::send(const char *_pb, size_t _bl, bool &_can_retry, ErrorCodeT &_rerr){
	return -1;
}

SocketDevice const& Socket::device()const{
	return sd;
}

int Socket::recvFrom(char *_pb, size_t _bl, SocketAddress &_addr, bool &_can_retry, ErrorCodeT &_rerr){
	return -1;
}

int Socket::sendTo(const char *_pb, size_t _bl, SocketAddressStub const &_rsas, bool &_can_retry, ErrorCodeT &_rerr){
	return -1;
}

bool Socket::secureAccept(bool &_can_retry, ErrorCodeT &_rerr){
	return false;
}

bool Socket::secureConnect(bool &_can_retry, ErrorCodeT &_rerr){
	return false;
}

bool Socket::secureShutdown(bool &_can_retry, ErrorCodeT &_rerr){
	return false;
}

}//namespace openssl
}//namespace aio
}//namespace frame
}//namespace solid



