// solid/system/socketinfo.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#ifdef SOLID_ON_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif
#include "solid/system/error.hpp"

namespace solid {

ErrorCodeT last_socket_error();

struct SocketInfo {
    enum Family {
        Local     = AF_UNIX,
        Inet4     = AF_INET,
        Inet6     = AF_INET6,
        AnyFamily = AF_UNSPEC,
    };
    enum Type {
        Stream   = SOCK_STREAM,
        Datagram = SOCK_DGRAM
    };
    enum {
        HostStringCapacity    = NI_MAXHOST,
        ServiceStringCapacity = NI_MAXSERV
    };
    //  //! Some request flags
    //  enum {
    //      NumericHost = NI_NUMERICHOST,   //!< Generate only numeric host
    //      NameRequest = NI_NAMEREQD,      //!< Force name lookup - fail if not found
    //      NumericService = NI_NUMERICSERV //!< Generate only the port number
    //  };
    static size_t max_listen_backlog_size();

private:
    SocketInfo();
    SocketInfo(const SocketInfo&);
    SocketInfo& operator=(const SocketInfo&);
};

} // namespace solid
