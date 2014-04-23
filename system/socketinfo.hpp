// system/socketinfo.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
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
#include "system/error.hpp"

namespace solid{

static ERROR_NS::error_code last_socket_error();

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

}//namespace solid

#endif
