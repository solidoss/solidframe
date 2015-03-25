// frame/aio/openssl/aiosecuresocket.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_OPENSSL_SECURE_SOCKET_HPP
#define SOLID_FRAME_AIO_OPENSSL_SECURE_SOCKET_HPP

#include "frame/aio/aiocommon.hpp"
#include "system/socketdevice.hpp"
#include <cerrno>

namespace solid{
namespace frame{
namespace aio{
namespace openssl{

class Context;

class	Socket{
public:
	
	Socket(Context &_rctx, SocketDevice &_rsd);
	
	Socket();
	
	~Socket();
	
	SocketDevice reset(Context &_rctx, SocketDevice &_rsd);
	
	void shutdown();
	
	bool create(Context &_rctx, SocketAddressStub const &_rsas);
	
	bool connect(SocketAddressStub const &_rsas, bool &_can_retry);
	
	ReactorEventsE filterReactorEvents(
		const  ReactorEventsE _evt,
		const bool /*_pending_recv*/,
		const bool /*_pendign_send*/
	) const;
	
	int recv(char *_pb, size_t _bl, bool &_can_retry);
	
	int send(const char *_pb, size_t _bl, bool &_can_retry);
	
	SocketDevice const& device()const;
	
	int recvFrom(char *_pb, size_t _bl, SocketAddress &_addr, bool &_can_retry);
	
	int sendTo(const char *_pb, size_t _bl, SocketAddressStub const &_rsas, bool &_can_retry);
	
	bool secureAccept(bool &_can_retry);
	bool secureConnect(bool &_can_retry);
	bool secureShutdown(bool &_can_retry);
private:
	SSL				*pssl;
	SocketDevice	sd;
};

}//namespace openssl
}//namespace aio
}//namespace frame
}//namespace solid


#endif
