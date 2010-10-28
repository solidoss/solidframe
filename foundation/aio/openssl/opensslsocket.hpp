/* Declarations file opensslsocket.hpp
	
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

#ifndef FOUNDATION_AIO_OPENSSL_SOCKET_HPP
#define FOUNDATION_AIO_OPENSSL_SOCKET_HPP

#include "foundation/aio/aiosecuresocket.hpp"
#include "openssl/ssl.h"

namespace foundation{

namespace aio{

namespace openssl{

class Socket;
//! A OpenSSL context wrapper
/*!
	NOTE: this is not a complete nor a final/perfect interface.
	The intention is to have an easy to use wrapper for
	OpenSSL. One can extend and improve the interface to offer
	more of OpenSSL functionality.
*/
class Context{
public:
	static Context* create();
	~Context();
	//! Creates a new OpenSSL SecureSocket
	Socket*	createSocket();
	
	//!Use it on client side to load the certificates
	int loadFile(const char *_path);
	//!Use it on client side to load the certificates
	int loadPath(const char *_path);
	
	//!Use it on server side to load the certificates
	int loadCertificateFile(const char *_path);
	//!Use it on server side to load the certificates
	int loadPrivateKeyFile(const char *_path);

private:
	Context(const Context&);
	Context& operator=(const Context&);
protected:
	Context(SSL_CTX *_pctx);
protected:
	SSL_CTX	*pctx;
};
//! A OpenSSL secure communication wrapper
/*!
	It is an implementation of foundation::aio::SecureSocket
	interface.
	
	NOTE: this is not a complete nor a final/perfect interface.
	The intention is to have an easy to use wrapper for
	OpenSSL. One can extend and improve the interface to offer
	more of OpenSSL functionality.
*/
class Socket: public SecureSocket{
public:
	/*virtual*/ ~Socket();
	/*virtual*/ void descriptor(const SocketDevice &);
	/*virtual*/ int send(const char *_pb, uint _bl, uint _flags = 0);
	/*virtual*/ int recv(char *_pb, uint _bl, uint _flags = 0);
	/*virtual*/ uint wantEvents()const;
	/*virtual*/ int secureAccept();
	/*virtual*/ int secureConnect();
private:

	friend class Context;	
	Socket& operator=(const Socket&);
	Socket(const Socket&);
protected:
	Socket(SSL *_pssl);
	bool shouldWait()const;

protected:
	SSL	*pssl;
};

}//namespace openssl

}//namespace aio

}//namespace foundation

#endif
