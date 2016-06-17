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

#include "frame/aio/aioerror.hpp"

#include <thread>
#include <mutex>
#include "system/cassert.hpp"
#include "system/debug.hpp"



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
	std::mutex	*pmtxes;
	
	 static void ssl_locking_fnc(int mode, int n, const char* /*file*/, int /*line*/){
		if (mode & CRYPTO_LOCK)
			the().pmtxes[n].lock();
		else
			the().pmtxes[n].unlock();
	}
	
	static unsigned long ssl_threadid_fnc()
	{	
		std::hash<std::thread::id> h;
		return h(std::this_thread::get_id());
	}
	
	Starter():pmtxes(nullptr){
		pmtxes = new std::mutex[::CRYPTO_num_locks()];
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

bool Context::isValid()const{
	return pctx != NULL;
}

Context::Context(Context && _rctx){
	pctx = _rctx.pctx;
	_rctx.pctx = NULL;
}
Context& Context::operator=(Context && _rctx){
	if(isValid()){
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
	if(isValid()){
		SSL_CTX_free(pctx);
		pctx = NULL;
	}
}


ErrorCodeT Context::loadFile(const char *_path){
	ErrorCodeT err;
	if(SSL_CTX_load_verify_locations(pctx, _path, NULL)){
		err = error_secure_context;
	}
	return err;
}
ErrorCodeT Context::loadPath(const char *_path){
	ErrorCodeT err;
	if(SSL_CTX_load_verify_locations(pctx, NULL, _path)){
		err = error_secure_context;
	}
	return err;
}

ErrorCodeT Context::loadCertificateFile(const char *_path){
	ErrorCodeT err;
	if(SSL_CTX_use_certificate_file(pctx, _path, SSL_FILETYPE_PEM)){
		err = error_secure_context;
	}
	return err;
}

ErrorCodeT Context::loadPrivateKeyFile(const char *_path){
	ErrorCodeT err;
	if(SSL_CTX_use_PrivateKey_file(pctx, _path, SSL_FILETYPE_PEM)){
		err = error_secure_context;
	}
	return err;
}

//=============================================================================


Socket::Socket(
	const Context &_rctx, SocketDevice &&_rsd
):sd(std::move(_rsd)), want_read_on_recv(false), want_read_on_send(false), want_write_on_recv(false), want_write_on_send(false){
	pssl = SSL_new(_rctx.pctx);
	::SSL_set_mode(pssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
	::SSL_set_mode(pssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
	if(sd.ok()){
		sd.makeNonBlocking();
		SSL_set_fd(pssl, sd.descriptor());
	}
}
	
Socket::Socket(const Context &_rctx): want_read_on_recv(false), want_read_on_send(false), want_write_on_recv(false), want_write_on_send(false){
	pssl = SSL_new(_rctx.pctx);
	::SSL_set_mode(pssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
	::SSL_set_mode(pssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
}

Socket::~Socket(){
	SSL_free(pssl);
}

SocketDevice Socket::reset(const Context &_rctx, SocketDevice &&_rsd, ErrorCodeT &_rerr){
	SocketDevice tmpsd = std::move(sd);
	sd = std::move(_rsd);
	if(sd.ok()){
		sd.makeNonBlocking();
		SSL_set_fd(pssl, sd.descriptor());
	}else{
		SSL_set_fd(pssl, -1);
	}
	return tmpsd;
}

void Socket::shutdown(){
	sd.shutdownReadWrite();
}

SocketDevice const& Socket::device()const{
	return sd;
}

SocketDevice& Socket::device(){
	return sd;
}

bool Socket::create(SocketAddressStub const &_rsas, ErrorCodeT &_rerr){
	_rerr = sd.create(_rsas.family());
	if(!_rerr){
		_rerr = sd.makeNonBlocking();
	}
	if(!_rerr){
		SSL_set_fd(pssl, sd.descriptor());
	}
	return !_rerr;
}

bool Socket::connect(SocketAddressStub const &_rsas, bool &_can_retry, ErrorCodeT &_rerr){
	_rerr = sd.connect(_rsas, _can_retry);
	return !_rerr;
}

ErrorCodeT Socket::renegotiate(){
	int rv = ::SSL_renegotiate(pssl);
	ErrorCodeT err;
	err.assign(rv, err.category());
	return err;
}

ReactorEventsE Socket::filterReactorEvents(
	const  ReactorEventsE _evt
) const{
	switch(_evt){
		case ReactorEventRecv:
			//idbgx(Debug::aio, "EventRecv "<<want_read_on_send<<' '<<want_read_on_recv<<' '<<want_write_on_send<<' '<<want_write_on_recv);
			if(want_read_on_send and want_read_on_recv){
				return ReactorEventSendRecv;
			}else if(want_read_on_send){
				return ReactorEventSend;
			}else if(want_read_on_recv){
				return ReactorEventRecv;
			}
			break;
		case ReactorEventSend:
			//idbgx(Debug::aio, "EventSend "<<want_read_on_send<<' '<<want_read_on_recv<<' '<<want_write_on_send<<' '<<want_write_on_recv);
			if(want_write_on_send and want_write_on_recv){
				return ReactorEventRecvSend;
			}else if(want_write_on_recv){
				return ReactorEventRecv;
			}else if(want_write_on_send){
				return ReactorEventSend;
			}
			break;
		case ReactorEventRecvSend:
			//idbgx(Debug::aio, "EventRecvSend "<<want_read_on_send<<' '<<want_read_on_recv<<' '<<want_write_on_send<<' '<<want_write_on_recv);
			if(want_read_on_send and (want_read_on_recv or want_write_on_recv)){
				return ReactorEventSendRecv;
			}else if((want_write_on_send or want_write_on_send) and (want_write_on_recv or want_read_on_recv)){
				return _evt;
			}else if(want_write_on_send or want_read_on_send){
				return ReactorEventSend;
			}else if(want_read_on_recv or want_write_on_recv){
				return ReactorEventRecv;
			}
			break;
		default:
			break;
	}
	return ReactorEventNone;
}

int Socket::recv(char *_pb, size_t _bl, bool &_can_retry, ErrorCodeT &_rerr){
	want_read_on_recv = want_write_on_recv = false;
	int rv = ::SSL_read(pssl, _pb, _bl);
	switch(::SSL_get_error(pssl, rv)){
		case SSL_ERROR_NONE:
			_can_retry = false;
			return rv;
		case SSL_ERROR_ZERO_RETURN:
			_can_retry = false;
			return 0;
		case SSL_ERROR_WANT_READ:
			_can_retry = true;
			want_read_on_recv = true;
			return -1;
		case SSL_ERROR_WANT_WRITE:
			_can_retry = true;
			want_write_on_recv = true;
			return -1;
		case SSL_ERROR_SYSCALL:
		case SSL_ERROR_SSL:
			_can_retry = false;
			_rerr = error_secure_recv;
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			//for reschedule, we can return -1 but not set the _rerr
		default:
			SOLID_ASSERT(false);
			break;
	}
	return -1;
}

int Socket::send(const char *_pb, size_t _bl, bool &_can_retry, ErrorCodeT &_rerr){
	want_read_on_send = want_write_on_send = false;
	int rv = ::SSL_write(pssl, _pb, _bl);
	switch(::SSL_get_error(pssl, rv)){
		case SSL_ERROR_NONE:
			_can_retry = false;
			return rv;
		case SSL_ERROR_ZERO_RETURN:
			_can_retry = false;
			return 0;
		case SSL_ERROR_WANT_READ:
			_can_retry = true;
			want_read_on_send = true;
			return -1;
		case SSL_ERROR_WANT_WRITE:
			_can_retry = true;
			want_write_on_send = true;
			return -1;
		case SSL_ERROR_SYSCALL:
		case SSL_ERROR_SSL:
			_can_retry = false;
			_rerr = error_secure_send;
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			//for reschedule, we can return -1 but not set the _rerr
		default:
			SOLID_ASSERT(false);
			break;
	}
	return -1;
}

bool Socket::secureAccept(bool &_can_retry, ErrorCodeT &_rerr){
	want_read_on_recv = want_write_on_recv = false;
	int rv = ::SSL_accept(pssl);
	switch(::SSL_get_error(pssl, rv)){
		case SSL_ERROR_NONE:
			_can_retry = false;
			return true;
		case SSL_ERROR_WANT_READ:
			_can_retry = true;
			want_read_on_recv = true;
			return false;
		case SSL_ERROR_WANT_WRITE:
			_can_retry = true;
			want_write_on_recv = true;
			return false;
		case SSL_ERROR_SYSCALL:
		case SSL_ERROR_SSL:
			_can_retry = false;
			_rerr = error_secure_accept;
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			//for reschedule, we can return -1 but not set the _rerr
		default:
			SOLID_ASSERT(false);
			break;
	}
	return false;
}

bool Socket::secureConnect(bool &_can_retry, ErrorCodeT &_rerr){
	want_read_on_send = want_write_on_send = false;
	int rv = ::SSL_connect(pssl);
	switch(::SSL_get_error(pssl, rv)){
		case SSL_ERROR_NONE:
			_can_retry = false;
			return true;
		case SSL_ERROR_WANT_READ:
			_can_retry = true;
			want_read_on_send = true;
			return false;
		case SSL_ERROR_WANT_WRITE:
			_can_retry = true;
			want_write_on_send = true;
			return false;
		case SSL_ERROR_SYSCALL:
		case SSL_ERROR_SSL:
			_can_retry = false;
			_rerr = error_secure_connect;
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			//for reschedule, we can return -1 but not set the _rerr
		default:
			SOLID_ASSERT(false);
			break;
	}
	return false;
}

bool Socket::secureShutdown(bool &_can_retry, ErrorCodeT &_rerr){
	want_read_on_send = want_write_on_send = false;
	int rv = ::SSL_shutdown(pssl);
	switch(::SSL_get_error(pssl, rv)){
		case SSL_ERROR_NONE:
			_can_retry = false;
			return true;
		case SSL_ERROR_WANT_READ:
			_can_retry = true;
			want_read_on_send = true;
			return false;
		case SSL_ERROR_WANT_WRITE:
			_can_retry = true;
			want_write_on_send = true;
			return false;
		case SSL_ERROR_SYSCALL:
		case SSL_ERROR_SSL:
			_can_retry = false;
			_rerr = error_secure_shutdown;
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			//for reschedule, we can return -1 but not set the _rerr
		default:
			SOLID_ASSERT(false);
			break;
	}
	return false;
}

int Socket::recvFrom(char *_pb, size_t _bl, SocketAddress &_addr, bool &_can_retry, ErrorCodeT &_rerr){
	return -1;
}

int Socket::sendTo(const char *_pb, size_t _bl, SocketAddressStub const &_rsas, bool &_can_retry, ErrorCodeT &_rerr){
	return -1;
}


}//namespace openssl
}//namespace aio
}//namespace frame
}//namespace solid



