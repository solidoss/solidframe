/* Implementation file opensslsocket.hpp
	
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

#ifndef OPENSSL_SOCKET_HPP
#define OPENSSL_SOCKET_HPP

#include "clientserver/aio/aiosecuresocket.hpp"
#include "openssl/ssl.h"

namespace clientserver{

namespace aio{

namespace openssl{

class Socket;

class Context{
public:
	static Context* create();
	~Context();
	Socket*	createSocket();
	int loadFile(const char *_path);
	int loadPath(const char *_path);
	int loadCertificateFile(const char *_path);
	int loadPrivateKeyFile(const char *_path);
private:
	Context(SSL_CTX *_pctx);
	
	Context(const Context&);
	Context& operator=(const Context&);
private:
	SSL_CTX	*pctx;
};

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
	Socket(SSL *_pssl);
	bool shouldWait()const;
	
	Socket& operator=(const Socket&);
	Socket(const Socket&);
private:
	SSL	*pssl;
};

}//namespace openssl

}//namespace aio

}//namespace clientserver

#endif
