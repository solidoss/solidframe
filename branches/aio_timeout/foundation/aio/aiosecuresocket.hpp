/* Declarations file aiosecuresocket.hpp
	
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

#ifndef AIO_SECURE_SOCKET_HPP
#define AIO_SECURE_SOCKET_HPP

#include "system/common.hpp"

class SocketDevice;

namespace foundation{

namespace aio{
//! Interface for secure socket (ssl sockets) used by aio::Socket
/*!
	This is the interface used by the aio::Socket for secure
	(ssl) communication. One have to implement the interface
	using its favorite ssl library.
	The default ssl library, used by SolidGround is OpenSSL
	(http://www.openssl.org/).
	See foundation/aio/openssl/opensslsocket.hpp for an implementation
	of SecureSocket interface as a wrapper for OpenSSL.
*/
class SecureSocket{
public:
	enum{
		WANT_READ = (1 << 0),
		WANT_WRITE = (1 << 1),
		WANT_READ_ON_ACCEPT = (1 << 2),
		WANT_WRITE_ON_ACCEPT = (1 << 3),
		WANT_READ_ON_CONNECT = (1 << 4),
		WANT_WRITE_ON_CONNECT = (1 << 5),
		WANT_READ_ON_READ = (1 << 6),
		WANT_WRITE_ON_READ = (1 << 7),
		WANT_READ_ON_WRITE = (1 << 8),
		WANT_WRITE_ON_WRITE = (1 << 9),
	};
	//! Virtual destructor
	virtual ~SecureSocket(){}
	//!Set the socket descriptor to be used by the secure socket
	virtual void descriptor(const SocketDevice &) = 0;
	//!Send method
	/*!
		The return value is modeled around OpenSSL API.
		
		\retval 0 for clean connection close, <0 for an error
		(for nonblocking sockets check wantEvents), >0 success.
	*/
	virtual int send(const char *_pb, uint _bl, uint _flags = 0) = 0;
	//!Receive method
	/*!
		The return value is modeled around OpenSSL API.
		
		\retval 0 for clean connection close, <0 for an error
		(for nonblocking sockets check wantEvents), >0 success.
	*/
	virtual int recv(char *_pb, uint _bl, uint _flags = 0) = 0;
	//! Return the events requested by the previous io opperation
	/*!
		The events can be: 0 or WANT_READ, or WANT_WRITE
		or WANT_READ | WANT_WRITE
	*/
	virtual uint wantEvents()const = 0;
	//! Do a secure accept
	/*!
		This is modeled around the OpenSSL API.
		It does the secure hand-shake for incomming
		connections. The socket has to be created.
		
		The return value is modeled around OpenSSL API.
		
		\retval 0 for clean connection close, <0 for an error
		(for nonblocking sockets check wantEvents), >0 success.
	*/
	virtual int secureAccept() = 0;
	//! Do a secure connect
	/*!
		This is modeled around the OpenSSL API.
		It does the secure hand-shake for outgoing
		connections.
		
		The return value is modeled around OpenSSL API.
		
		\retval 0 for clean connection close, <0 for an error
		(for nonblocking sockets check wantEvents), >0 success.
	*/
	virtual int secureConnect() = 0;
};

}//namespace aio

}//namespace foundation


#endif
