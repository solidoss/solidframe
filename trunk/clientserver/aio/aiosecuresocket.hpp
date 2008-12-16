/* Declarations file aiosocket.hpp
	
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

namespace clientserver{

namespace aio{

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
	virtual ~SecureSocket(){}
	virtual void descriptor(const SocketDevice &) = 0;
	virtual int send(const char *_pb, uint _bl, uint _flags = 0) = 0;
	virtual int recv(char *_pb, uint _bl, uint _flags = 0) = 0;
	virtual uint wantEvents()const = 0;
	virtual int secureAccept() = 0;
	virtual int secureConnect() = 0;
};

}//namespace aio

}//namespace clientserver


#endif
