/* Implementation file opensslsocket.cpp
	
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

#include "foundation/aio/openssl/opensslsocket.hpp"
#include "system/socketdevice.hpp"
#include "system/common.hpp"

#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"

namespace foundation{

namespace aio{

namespace openssl{

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
	//TODO: staticproblem
	static Initor init;
	SSL_CTX *pctx(SSL_CTX_new(SSLv23_server_method()));
	if(!pctx) return NULL;
	return new Context(pctx);
}
Context::~Context(){
	SSL_CTX_free(pctx);
}
Socket*	Context::createSocket(){
	SSL *pssl(SSL_new(pctx));
	if(!pssl) return NULL;
	return new Socket(pssl);
}
int Context::loadFile(const char *_path){
	if(SSL_CTX_load_verify_locations(pctx, _path, NULL)) return BAD;
	return OK;
}
int Context::loadPath(const char *_path){
	if(SSL_CTX_load_verify_locations(pctx, NULL, _path)) return BAD;
	return OK;
}
int Context::loadCertificateFile(const char *_path){
	if(SSL_CTX_use_certificate_file(pctx, _path, SSL_FILETYPE_PEM)) return BAD;
	return OK;
}
int Context::loadPrivateKeyFile(const char *_path){
	if(SSL_CTX_use_PrivateKey_file(pctx, _path, SSL_FILETYPE_PEM)) return BAD;
	return OK;
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
/*virtual*/ int Socket::secureAccept(){
	int rv = SSL_accept(pssl);
	if(rv == 0) return BAD;
	if(rv > 0) return OK;
	if(shouldWait()){
		return NOK;
	}return BAD;
}
/*virtual*/ int Socket::secureConnect(){
	int rv = SSL_accept(pssl);
	if(rv == 0) return BAD;
	if(rv > 0) return OK;
	if(shouldWait()){
		return NOK;
	}return BAD;
}
Socket::Socket(SSL *_pssl):pssl(_pssl){
}

}//namespace openssl

}//namespace aio

}//namespace foundation

