// solid/frame/aio/openssl/src/aiocompletion.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/frame/aio/openssl/aiosecuresocket.hpp"
#include "solid/frame/aio/openssl/aiosecurecontext.hpp"

#include "solid/frame/aio/aioerror.hpp"

#include <thread>
#include <mutex>
#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/debug.hpp"
#include "solid/system/error.hpp"

//#include <cstdio>

#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/conf.h"

namespace{
class ErrorCategory: public solid::ErrorCategoryT
{     
public:
	ErrorCategory(){} 
	const char* name() const noexcept{
		return "OpenSSL";
	}
	std::string message(int _ev)const;
	
	solid::ErrorCodeT makeError(int _err)const{
		return solid::ErrorCodeT(_err, *this);
	}
};

const ErrorCategory ssl_category;


std::string ErrorCategory::message(int _ev) const{
	std::ostringstream	oss;
	char				buf[1024];
	
	ERR_error_string_n(_ev, buf, 1024);
	
	oss<<"("<<name()<<":"<<_ev<<"): "<<buf;
	return oss.str();
}

}//namespace

namespace solid{
namespace frame{
namespace aio{
namespace openssl{


struct Starter{
	
	Starter(){
		::OPENSSL_init_ssl(0,NULL);
		::OPENSSL_init_crypto(0, NULL);
		::OpenSSL_add_all_algorithms();
	}
	~Starter(){
#ifndef OPENSSL_IS_BORINGSSL
		::CONF_modules_unload(1);
#endif
	}
	
	static Starter& the(){
		static Starter s;
		return s;
	}
	
};

//=============================================================================

/*static*/ Context Context::create(const SSL_METHOD* _pm/*= nullptr*/){
	Starter::the();
	Context	rv;
	if(_pm == nullptr){
		rv.pctx = ::SSL_CTX_new(::TLS_method());
	}else{
		rv.pctx = ::SSL_CTX_new(_pm);
	}
	return rv;
}

bool Context::isValid()const{
	return pctx != nullptr;
}

Context::Context(Context && _rctx){
	pctx = _rctx.pctx;
	_rctx.pctx = nullptr;
}
Context& Context::operator=(Context && _rctx){
	if(isValid()){
		SSL_CTX_free(pctx);
		pctx = nullptr;
	}
	pctx = _rctx.pctx;
	_rctx.pctx = nullptr;
	return *this;
}
Context::Context():pctx(nullptr){
	
}
Context::~Context(){
	if(isValid()){
		SSL_CTX_free(pctx);
		pctx = nullptr;
	}
}


ErrorCodeT Context::loadFile(const char *_path){
	ErrorCodeT err;
	if(SSL_CTX_load_verify_locations(pctx, _path, nullptr)){
		err = error_secure_context;
	}
	return err;
}
ErrorCodeT Context::loadPath(const char *_path){
	ErrorCodeT err;
	if(SSL_CTX_load_verify_locations(pctx, nullptr, _path)){
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

/*static*/ int Socket::thisSSLDataIndex(){
	static int idx =  SSL_get_ex_new_index(0, (void*)"socket_data", nullptr, nullptr, nullptr);
	return idx;
}
/*static*/ int Socket::contextPointerSSLDataIndex(){
	static int idx =  SSL_get_ex_new_index(0, (void*)"context_data", nullptr, nullptr, nullptr);
	return idx;
}

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
	return ssl_category.makeError(::SSL_get_error(pssl, rv));
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
	return _evt;
}

int Socket::recv(void *_pctx, char *_pb, size_t _bl, bool &_can_retry, ErrorCodeT &_rerr){
	want_read_on_recv = want_write_on_recv = false;
	
	storeThisPointer();
	storeContextPointer(_pctx);
	
	const int rv = ::SSL_read(pssl, _pb, _bl);
	const int err = ::SSL_get_error(pssl, rv);
	
	clearThisPointer();
	clearContextPointer();
	
	switch(err){
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
			_rerr = ssl_category.makeError(err);
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			//for reschedule, we can return -1 but not set the _rerr
		default:
			SOLID_ASSERT(false);
			break;
	}
	return -1;
}

int Socket::send(void *_pctx, const char *_pb, size_t _bl, bool &_can_retry, ErrorCodeT &_rerr){
	want_read_on_send = want_write_on_send = false;
	
	storeThisPointer();
	storeContextPointer(_pctx);
	
	const int rv  = ::SSL_write(pssl, _pb, _bl);
	const int err = ::SSL_get_error(pssl, rv);
	
	clearThisPointer();
	clearContextPointer();
	
	switch(err){
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
			_rerr = ssl_category.makeError(err);
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			//for reschedule, we can return -1 but not set the _rerr
		default:
			SOLID_ASSERT(false);
			break;
	}
	return -1;
}

bool Socket::secureAccept(void *_pctx, bool &_can_retry, ErrorCodeT &_rerr){
	want_read_on_recv = want_write_on_recv = false;
	
	storeThisPointer();
	storeContextPointer(_pctx);
	
	const int rv  = ::SSL_accept(pssl);
	const int err = ::SSL_get_error(pssl, rv);
	
	clearThisPointer();
	clearContextPointer();
	
	switch(err){
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
			_rerr = ssl_category.makeError(err);
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			//for reschedule, we can return -1 but not set the _rerr
		default:
			SOLID_ASSERT(false);
			break;
	}
	return false;
}

bool Socket::secureConnect(void *_pctx, bool &_can_retry, ErrorCodeT &_rerr){
	want_read_on_send = want_write_on_send = false;
	
	storeThisPointer();
	storeContextPointer(_pctx);
	
	const int rv  = ::SSL_connect(pssl);
	const int err = ::SSL_get_error(pssl, rv);//ERR_print_errors_fp(stdout);
	
	clearThisPointer();
	clearContextPointer();
	
	vdbgx(Debug::aio, "ssl_connect rv = "<<rv<<" ssl_error "<<err);
	
	switch(err){
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
			_rerr = ssl_category.makeError(err);
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			//for reschedule, we can return -1 but not set the _rerr
		default:
			SOLID_ASSERT(false);
			break;
	}
	return false;
}

bool Socket::secureShutdown(void *_pctx, bool &_can_retry, ErrorCodeT &_rerr){
	want_read_on_send = want_write_on_send = false;
	
	storeThisPointer();
	storeContextPointer(_pctx);
	
	const int rv  = ::SSL_shutdown(pssl);
	const int err = ::SSL_get_error(pssl, rv);
	
	
	clearThisPointer();
	clearContextPointer();
	
	switch(err){
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
			_rerr = ssl_category.makeError(err);
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


void Socket::storeContextPointer(void *_pctx){
	if(pssl){
		SSL_set_ex_data(pssl, contextPointerSSLDataIndex(), _pctx);
	}
}

void Socket::clearContextPointer(){
	if(pssl){
		SSL_set_ex_data(pssl, contextPointerSSLDataIndex(), nullptr);
	}
}

void Socket::storeThisPointer(){
	SSL_set_ex_data(pssl, thisSSLDataIndex(), this);
}

void Socket::clearThisPointer(){
	SSL_set_ex_data(pssl, thisSSLDataIndex(), nullptr);
}

static int convertMask(const VerifyMaskT _verify_mask){
	int rv = 0;
	if(_verify_mask & VerifyModeNone) rv |= SSL_VERIFY_NONE;
	if(_verify_mask & VerifyModePeer) rv |= SSL_VERIFY_PEER;
	if(_verify_mask & VerifyModeFailIfNoPeerCert) rv |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
	if(_verify_mask & VerifyModeClientOnce) rv |= SSL_VERIFY_CLIENT_ONCE;
	return rv;
}

ErrorCodeT Socket::doPrepareVerifyCallback(VerifyMaskT _verify_mask){
	SSL_set_verify(pssl, convertMask(_verify_mask), on_verify);
	
	ErrorCodeT err;
	return err;
}

/*static*/ int Socket::on_verify(int preverify_ok, X509_STORE_CTX *x509_ctx){
	SSL 	*ssl = static_cast<SSL*>(X509_STORE_CTX_get_ex_data(x509_ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
	Socket	*pthis = static_cast<Socket*>(SSL_get_ex_data(ssl, thisSSLDataIndex()));
	void	*pctx = SSL_get_ex_data(ssl, contextPointerSSLDataIndex());
	
	SOLID_CHECK(ssl);
	SOLID_CHECK(pthis);
	
	if(not FUNCTION_EMPTY(pthis->verify_cbk)){
		VerifyContext	vctx(x509_ctx);
		
		bool rv = pthis->verify_cbk(pctx, preverify_ok != 0, vctx);
		return rv ? 1 : 0;
	}else{
		return preverify_ok;
	}
}

ErrorCodeT Socket::secureVerifyDepth(const int _depth){
	SSL_set_verify_depth(pssl, _depth);
	ErrorCodeT err;
	return err;
}
	
ErrorCodeT Socket::secureVerifyMode(VerifyMaskT _verify_mask){
	SSL_set_verify(pssl, convertMask(_verify_mask), on_verify);
	ErrorCodeT err;
	return err;
}

}//namespace openssl
}//namespace aio
}//namespace frame
}//namespace solid



