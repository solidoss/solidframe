// solid/frame/aio/openssl/aiosecuresocket.hpp
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

#include "solid/frame/aio/aiocommon.hpp"
#include "solid/system/socketdevice.hpp"
#include "solid/system/function.hpp"
#include "openssl/ssl.h"
#include <cerrno>

namespace solid{
namespace frame{
namespace aio{	

struct ReactorContext;

namespace openssl{

class Context;

enum VerifyMode{
	VerifyModeNone 				= 1,
	VerifyModePeer				= 2,
	VerifyModeFailIfNoPeerCert	= 4,
	VerifyModeClientOnce		= 8,
};

using VerifyMaskT = int;

class Socket;

struct VerifyContext{
	using NativeContextT = X509_STORE_CTX;
	
	NativeContextT* nativeHandle()const{
		return ssl_ctx;
	}
	
	VerifyContext(const VerifyContext&) = delete;
	VerifyContext(VerifyContext &&) = delete;
	
	VerifyContext& operator=(VerifyContext &&) = delete;
	
private:
	friend class Socket;
	VerifyContext(NativeContextT *_ssl_ctx):ssl_ctx(_ssl_ctx){}
private:
	
	NativeContextT	*ssl_ctx;
};

class	Socket{
public:
	
	
	using NativeHandleT = SSL*;
	using VerifyMaskT = openssl::VerifyMaskT;
	using VerifyContextT = openssl::VerifyContext;
	
	Socket(const Context &_rctx, SocketDevice &&_rsd);
	
	Socket(const Context &_rctx);
	
	~Socket();
	
	SocketDevice reset(const Context &_rctx, SocketDevice &&_rsd, ErrorCodeT &_rerr);
	
	void shutdown();
	
	bool create(SocketAddressStub const &_rsas, ErrorCodeT &_rerr);
	
	bool connect(SocketAddressStub const &_rsas, bool &_can_retry, ErrorCodeT &_rerr);
	
	
	ErrorCodeT renegotiate(bool &_can_retry);
	
	ReactorEventsE filterReactorEvents(
		const  ReactorEventsE _evt
	) const;
	
	int recv(void *_pctx, char *_pb, size_t _bl, bool &_can_retry, ErrorCodeT &_rerr);
	
	int send(void *_pctx, const char *_pb, size_t _bl, bool &_can_retry, ErrorCodeT &_rerr);
	
	SocketDevice const& device()const;
	SocketDevice& device();
	
	NativeHandleT nativeHandle()const;
	
	int recvFrom(char *_pb, size_t _bl, SocketAddress &_addr, bool &_can_retry, ErrorCodeT &_rerr);
	
	int sendTo(const char *_pb, size_t _bl, SocketAddressStub const &_rsas, bool &_can_retry, ErrorCodeT &_rerr);
	
	bool secureAccept(void *_pctx, bool &_can_retry, ErrorCodeT &_rerr);
	bool secureConnect(void *_pctx, bool &_can_retry, ErrorCodeT &_rerr);
	bool secureShutdown(void *_pctx, bool &_can_retry, ErrorCodeT &_rerr);
	
	template <typename Cbk>
	ErrorCodeT setVerifyCallback(VerifyMaskT _verify_mask, Cbk _cbk){
		verify_cbk = _cbk;
		return doPrepareVerifyCallback(_verify_mask);
	}
	
	ErrorCodeT setVerifyDepth(const int _depth);
	
	ErrorCodeT setVerifyMode(VerifyMaskT _verify_mask);
	
	ErrorCodeT setCheckHostName(const std::string &_hostname);
	ErrorCodeT setCheckEmail(const std::string &_hostname);
	ErrorCodeT setCheckIP(const std::string &_hostname);
	
private:
	static int thisSSLDataIndex();
	static int contextPointerSSLDataIndex();
	
	void storeThisPointer();
	void clearThisPointer();
	
	void storeContextPointer(void *_pctx);
	void clearContextPointer();
	
	ErrorCodeT doPrepareVerifyCallback(VerifyMaskT _verify_mask);
	
	static int on_verify(int preverify_ok, X509_STORE_CTX *x509_ctx);
private:
	using VerifyFunctionT = FUNCTION<bool(void *, bool, VerifyContextT&)>;
	
	SSL					*pssl;
	SocketDevice		sd;
	
	bool				want_read_on_recv;
	bool				want_read_on_send;
	bool				want_write_on_recv;
	bool				want_write_on_send;
	VerifyFunctionT		verify_cbk;
};

inline Socket::NativeHandleT Socket::nativeHandle()const{
	return pssl;
}

}//namespace openssl
}//namespace aio
}//namespace frame
}//namespace solid


#endif
