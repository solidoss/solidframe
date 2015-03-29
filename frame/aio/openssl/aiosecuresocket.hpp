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
#include "openssl/ssl.h"
#include <cerrno>

namespace solid{
namespace frame{
namespace aio{
namespace openssl{

class Context;

class	Socket{
public:
	
	Socket(Context &_rctx, SocketDevice &&_rsd);
	
	Socket(Context &_rctx);
	
	~Socket();
	
	SocketDevice reset(Context &_rctx, SocketDevice &&_rsd, ErrorCodeT &_rerr);
	
	void shutdown();
	
	bool create(SocketAddressStub const &_rsas, ErrorCodeT &_rerr);
	
	bool connect(SocketAddressStub const &_rsas, bool &_can_retry, ErrorCodeT &_rerr);
	
	
	ErrorCodeT renegotiate();
	
	ReactorEventsE filterReactorEvents(
		const  ReactorEventsE _evt
	) const;
	
	int recv(char *_pb, size_t _bl, bool &_can_retry, ErrorCodeT &_rerr);
	
	int send(const char *_pb, size_t _bl, bool &_can_retry, ErrorCodeT &_rerr);
	
	SocketDevice const& device()const;
	SocketDevice& device();
	
	int recvFrom(char *_pb, size_t _bl, SocketAddress &_addr, bool &_can_retry, ErrorCodeT &_rerr);
	
	int sendTo(const char *_pb, size_t _bl, SocketAddressStub const &_rsas, bool &_can_retry, ErrorCodeT &_rerr);
	
	bool secureAccept(bool &_can_retry, ErrorCodeT &_rerr);
	bool secureConnect(bool &_can_retry, ErrorCodeT &_rerr);
	bool secureShutdown(bool &_can_retry, ErrorCodeT &_rerr);
private:
	SSL				*pssl;
	SocketDevice	sd;
	
	bool			want_read_on_recv;
	bool			want_read_on_send;
	bool			want_write_on_recv;
	bool			want_write_on_send;
};

}//namespace openssl
}//namespace aio
}//namespace frame
}//namespace solid


#endif
