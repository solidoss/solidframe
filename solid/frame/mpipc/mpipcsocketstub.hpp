// solid/frame/ipc/ipcsocketstub.hpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_MPIPC_MPIPC_SOCKET_STUB_HPP
#define SOLID_FRAME_MPIPC_MPIPC_SOCKET_STUB_HPP

#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/frame/common.hpp"

namespace solid{
namespace frame{
namespace mpipc{

class SocketStub{
public:
	typedef void (*OnSendAllRawF)(frame::aio::ReactorContext &, Event &);
	typedef void (*OnRecvSomeRawF)(frame::aio::ReactorContext &, const size_t , Event &);
	typedef void (*OnConnectF)(frame::aio::ReactorContext &);
	typedef void (*OnRecvF)(frame::aio::ReactorContext &, size_t);
	typedef void (*OnSendF)(frame::aio::ReactorContext &);
	
	static void emplace_deleter(SocketStub *_pss){
		_pss->~SocketStub();
	}
	
	static void delete_deleter(SocketStub *_pss){
		delete _pss;
	}
	
	virtual ~SocketStub(){
		
	}
	
	virtual bool postSendAll(
		frame::aio::ReactorContext &_rctx, OnSendAllRawF _pf, const char *_pbuf, size_t _bufcp, Event &_revent
	) = 0;
	
	
	virtual bool postRecvSome(
		frame::aio::ReactorContext &_rctx, OnRecvF _pf,  char *_pbuf, size_t _bufcp
	) = 0;
	
	virtual bool postRecvSome(
		frame::aio::ReactorContext &_rctx, OnRecvSomeRawF _pf, char *_pbuf, size_t _bufcp, Event &_revent
	) = 0;
	
	virtual bool hasValidSocket() const = 0;
	
	virtual bool connect(
		frame::aio::ReactorContext &_rctx, OnConnectF _pf, const SocketAddressInet&_raddr
	) = 0;
	
	virtual bool recvSome(
		frame::aio::ReactorContext &_rctx, OnRecvF _pf, char *_buf, size_t _bufcp, size_t &_sz
	) = 0;
	
	virtual bool hasPendingSend() const = 0;
	
	virtual bool sendAll(
		frame::aio::ReactorContext &_rctx, OnSendF _pf, char *_buf, size_t _bufcp
	) = 0;
	
	virtual void prepareSocket(
		frame::aio::ReactorContext &_rctx
	) = 0;
};

typedef void(*SocketStubDeleteF)(SocketStub *);

using SocketStubPtrT = std::unique_ptr<SocketStub, SocketStubDeleteF>;

}//namespace mpipc
}//namespace frame
}//namespace solid

#endif
