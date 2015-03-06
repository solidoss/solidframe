// frame/aio/openssl/src/opensslsocket.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/aio/openssl/opensslsocket.hpp"
#include "system/socketdevice.hpp"
#include "system/common.hpp"

#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"

namespace solid{
namespace frame{
namespace aio{
namespace openssl{

#ifdef HAS_SAFE_STATIC

struct Initor{
	Initor();
	~Initor();
};

Initor::Initor(){
	SSL_library_init();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();
}
Initor::~Initor(){
}


/*static*/ Context* Context::create(){
	static Initor init;
	SSL_CTX *pctx(SSL_CTX_new(SSLv23_server_method()));
	if(!pctx) return NULL;
	return new Context(pctx);
}
#else

void once_initor(){
	SSL_library_init();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();
}
/*static*/ Context* Context::create(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_initor, once);
	SSL_CTX *pctx(SSL_CTX_new(SSLv23_server_method()));
	if(!pctx) return NULL;
	return new Context(pctx);
}


#endif
Context::~Context(){
	SSL_CTX_free(pctx);
}
Socket*	Context::createSocket(){
	SSL *pssl(SSL_new(pctx));
	if(!pssl) return NULL;
	return new Socket(pssl);
}
bool Context::loadFile(const char *_path){
	if(SSL_CTX_load_verify_locations(pctx, _path, NULL)) return false;
	return true;
}
bool Context::loadPath(const char *_path){
	if(SSL_CTX_load_verify_locations(pctx, NULL, _path)) return false;
	return true;
}
bool Context::loadCertificateFile(const char *_path){
	if(SSL_CTX_use_certificate_file(pctx, _path, SSL_FILETYPE_PEM)) return false;
	return true;
}
bool Context::loadPrivateKeyFile(const char *_path){
	if(SSL_CTX_use_PrivateKey_file(pctx, _path, SSL_FILETYPE_PEM)) return false;
	return true;
}
Context::Context(SSL_CTX *_pctx):pctx(_pctx){
}
//============================================================================
inline bool Socket::shouldWait()const{
	return SSL_want(pssl) != SSL_NOTHING;
}
Socket::~Socket(){
	SSL_free(pssl);
}
/*virtual*/ void Socket::descriptor(const SocketDevice &_sd){
	SSL_set_fd(pssl, _sd.descriptor());
}
/*virtual*/ int Socket::send(const char *_pb, uint _bl, uint _flags){
	return SSL_write(pssl, _pb, _bl);
}
/*virtual*/ int Socket::recv(char *_pb, uint _bl, uint _flags){
	return SSL_read(pssl, _pb, _bl);
}
/*virtual*/ uint Socket::wantEvents()const{
	uint rv = 0;
	if(SSL_want_read(pssl)){
		rv |= WANT_READ;
	}
	if(SSL_want_write(pssl)){
		rv |= WANT_WRITE;
	}
	return rv;
}
/*virtual*/ AsyncE Socket::secureAccept(){
	int rv = SSL_accept(pssl);
	if(rv > 0) return AsyncSuccess;
	if(rv == 0) return AsyncError;
	if(shouldWait()){
		return AsyncWait;
	}return AsyncError;
}
/*virtual*/ AsyncE Socket::secureConnect(){
	int rv = SSL_connect(pssl);
	if(rv > 0) return AsyncSuccess;
	if(rv == 0) return AsyncError;
	if(shouldWait()){
		return AsyncWait;
	}return AsyncError;
}
Socket::Socket(SSL *_pssl):pssl(_pssl){
}

}//namespace openssl
}//namespace aio
}//namespace frame
}//namespace solid

