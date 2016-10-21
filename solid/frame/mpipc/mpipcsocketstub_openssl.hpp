// solid/frame/ipc/ipcsocketstub.hpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_MPIPC_MPIPC_SOCKET_STUB_OPENSSL_HPP
#define SOLID_FRAME_MPIPC_MPIPC_SOCKET_STUB_OPENSSL_HPP

#include "solid/system/socketdevice.hpp"

#include "solid/utility/event.hpp"

#include "solid/frame/aio/openssl/aiosecuresocket.hpp"
#include "solid/frame/aio/openssl/aiosecurecontext.hpp"
#include "solid/frame/aio/aiostream.hpp"

#include "solid/frame/mpipc/mpipcsocketstub.hpp"

namespace solid{
namespace frame{
namespace mpipc{
namespace openssl{
	
using ContextT	= frame::aio::openssl::Context;

struct Configuration{
	Configuration(){}
	
	ContextT		server_context;
	ContextT		client_context;
};

class SocketStub: public SocketStub{
public:
	SocketStub(f
		rame::aio::ObjectProxy const &_rproxy, ContextT &_rsecure_ctx
	):sock(_rproxy, _rsecure_ctx){}
	
	SocketStub(
		frame::aio::ObjectProxy const &_rproxy, SocketDevice &&_usd, ContextT &_rsecure_ctx
	):sock(_rproxy, std::move(_usd), _rsecure_ctx){}
	
private:
	~SecureSocketStub(){
		
	}
	
	bool postSendAll(
		frame::aio::ReactorContext &_rctx, OnSendAllRawF _pf, const char *_pbuf, size_t _bufcp, Event &_revent
	) override final{
		struct Closure{
			OnSendAllRawF	pf;
			Event 			event;
			
			Closure(OnSendAllRawF _pf, Event const &_revent):pf(_pf), event(_revent){
			}
			
			void operator()(frame::aio::ReactorContext &_rctx){
				(*pf)(_rctx, event);
			}
			
		} lambda(_pf, _revent);
	
		//TODO: find solution for costly event copy
	
		return sock.postSendAll(_rctx, _pbuf, _bufcp, lambda);
	}
	
	
	bool postRecvSome(
		frame::aio::ReactorContext &_rctx, OnRecvF _pf,  char *_pbuf, size_t _bufcp
	) override final{
		return sock.postRecvSome(_rctx, _pbuf, _bufcp, _pf);
	}
	
	bool postRecvSome(
		frame::aio::ReactorContext &_rctx, OnRecvSomeRawF _pf, char *_pbuf, size_t _bufcp, Event &_revent
	) override final{
		struct Closure{
			OnRecvSomeRawF	pf;
			Event			event;
			
			Closure(OnRecvSomeRawF _pf, Event const &_revent):pf(_pf), event(_revent){
			}
			
			void operator()(frame::aio::ReactorContext &_rctx, size_t _sz){
				(*pf)(_rctx, _sz, event);
			}
			
		} lambda(_pf, _revent);
		
		//TODO: find solution for costly event copy
		
		return sock.postRecvSome(_rctx, _pbuf, _bufcp, lambda);
	}
	
	bool hasValidSocket() const override final{
		return sock.device().ok();
	}
	
	bool connect(
		frame::aio::ReactorContext &_rctx, OnConnectF _pf, const SocketAddressInet&_raddr
	) override final{
		return sock.connect(_rctx, _raddr, _pf);
	}
	
	bool recvSome(
		frame::aio::ReactorContext &_rctx, OnRecvF _pf, char *_buf, size_t _bufcp, size_t &_sz
	) override final{
		return sock.recvSome(_rctx, _buf, _bufcp, _pf, _sz);
	}
	
	bool hasPendingSend() const override final{
		return sock.hasPendingSend();
	}
	
	bool sendAll(
		frame::aio::ReactorContext &_rctx, OnSendF _pf, char *_buf, size_t _bufcp
	) override final{
		return sock.sendAll(_rctx, _buf, _bufcp, _pf);
	}
	
	void prepareSocket(
		frame::aio::ReactorContext &_rctx
	) override final{
		sock.device().enableNoSignal();
	}
	
	StreamSocketT& socket(){
		return sock;
	}
	StreamSocketT const& socket()const{
		return sock;
	}
private:
	using StreamSocketT = frame::aio::Stream<frame::aio::openssl::Socket>;
	
	StreamSocketT				sock;
};

inline SocketStubPtrT create_connecting_socket(Configuration const &_rcfg, frame::aio::ObjectProxy const &_rproxy, char *_emplace_buf){
		
	if(sizeof(PlainSocketStub) > static_cast<size_t>(ConnectionValues::SocketEmplacementSize)){
		return SocketStubPtrT(new SecureSocketStub(_rproxy, _rcfg.secure_context_any.cast<SecureConfiguration>().client_context), SocketStub::delete_deleter);
	}else{
		return SocketStubPtrT(new(_emplace_buf) SecureSocketStub(_rproxy, _rcfg.secure_context_any.cast<SecureConfiguration>().client_context), SocketStub::emplace_deleter);
	}
}

inline SocketStubPtrT create_accepted_socket(Configuration const &/*_rcfg*/, frame::aio::ObjectProxy const &_rproxy, SocketDevice &&_usd, char *_emplace_buf){
		
	if(sizeof(PlainSocketStub) > static_cast<size_t>(ConnectionValues::SocketEmplacementSize)){
		return SocketStubPtrT(new SecureSocketStub(_rproxy, std::move(_usd), _rcfg.secure_context_any.cast<SecureConfiguration>().server_context), SocketStub::delete_deleter);
	}else{
		return SocketStubPtrT(new(_emplace_buf) SecureSocketStub(_rproxy, std::move(_usd), _rcfg.secure_context_any.cast<SecureConfiguration>().server_context), SocketStub::emplace_deleter);
	}
}

namespace impl{

template <typename F>
struct OnSecureVerifyFunctor{
	
	bool	ignore_certificate_error;
	
	bool operator()(frame::aio::ReactorContext &_rctx, bool _preverified, frame::aio::openssl::VerifyContext &_rverify_ctx){
		
	}
	
};


inline bool basic_on_secure_verify(frame::aio::ReactorContext &_rctx, bool _preverified, frame::aio::openssl::VerifyContext &_rverify_ctx){
	
}

inline bool basic_secure_accept(ConnectionContext& _rctx, frame::aio::ReactorContext &_rctx, SocketStubPtrT &_rsock_ptr, OnSecureAcceptF _pf, ErrorCodeT& _rerr){
	SocketStub &rss = *static_cast<SocketStub*>(_rsock_ptr.get());
	rss.socket().secureSetVerifyCallback(_rctx, frame::aio::openssl::VerifyModePeer, basic_on_secure_verify);
}

inline bool basic_secure_connect(ConnectionContext& _rctx, frame::aio::ReactorContext &_rctx, SocketStubPtrT &_rsock_ptr, OnSecureConnectF _pf, ErrorCodeT& _rerr){
	
}


}//namespace impl

enum ClientSetupFlags{
	ClientSetupIgnoreCertificateErrors = 1,
	ClientSetupNoCheckServerName = 2
};

template <class ContextSetupFunction, class ConnectionStartSecureFunction>
inline void setup_client_only(
	mpipc::Configuration &_rcfg,
	const ContextSetupFunction &_ctx_fnc,
	ConnectionStartSecureFunction &&_start_fnc
){
	_rcfg.secure_any = make_any<Configuration>();
	Configuration &rsecure_cfg = *_rcfg.secure_any.cast<Configuration>();
	
	rsecure_cfg.client_context = ContextT::create();
	
	_ctx_fnc(rsecure_cfg.client_context);
	
	_rcfg.connection_create_connecting_socket_fnc = create_connecting_socket;
	//_rcfg.connection_create_accepted_socket_fnc = create_accepted_socket_secure;
	//_rcfg.connection_start_secure_accept_fnc = 
	_rcfg.connection_start_secure_connect_fnc = 
}


}//namespace openssl
}//namespace mpipc
}//namespace frame
}//namespace solid

#endif

