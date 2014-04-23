// frame/aio/openssl/opensslsocket.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_OPENSSL_SOCKET_HPP
#define SOLID_FRAME_AIO_OPENSSL_SOCKET_HPP

#include "frame/aio/aiosecuresocket.hpp"
#include "openssl/ssl.h"

namespace solid{
namespace frame{
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
	bool loadFile(const char *_path);
	//!Use it on client side to load the certificates
	bool loadPath(const char *_path);
	
	//!Use it on server side to load the certificates
	bool loadCertificateFile(const char *_path);
	//!Use it on server side to load the certificates
	bool loadPrivateKeyFile(const char *_path);

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
	/*virtual*/ AsyncE secureAccept();
	/*virtual*/ AsyncE secureConnect();
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
}//namespace frame
}//namespace solid

#endif
