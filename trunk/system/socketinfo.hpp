/* Declarations file socketinfo.hpp
	
	Copyright 2012 Valentin Palade 
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

#ifndef SYSTEM_SOCKET_INFO_HPP
#define SYSTEM_SOCKET_INFO_HPP

#ifdef ON_WINDOWS
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Windows.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

struct SocketInfo{
	enum Family{
		Local = AF_UNIX,
		Inet4 = AF_INET,
		Inet6 = AF_INET6
	};
	enum Type{
		Stream = SOCK_STREAM,
		Datagram = SOCK_DGRAM
	};
	enum {HostStringCapacity = NI_MAXHOST};
	enum {ServiceStringCapacity = NI_MAXSERV};
	//! Some request flags
	enum {
		NumericHost = NI_NUMERICHOST,	//!< Generate only numeric host
		NameRequest = NI_NAMEREQD,		//!< Force name lookup - fail if not found
		NumericService = NI_NUMERICSERV	//!< Generate only the port number
	};
private:
	SocketInfo();
	SocketInfo(const SocketInfo&);
	SocketInfo& operator=(const SocketInfo&);
};

#endif
