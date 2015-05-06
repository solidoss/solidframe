// frame/ipc/src/ipcsession.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/manager.hpp"
#include "ipcsession.hpp"
#include "frame/ipc/ipcservice.hpp"
#include "frame/aio/aioreactorcontext.hpp"
#include "frame/aio/openssl/aiosecuresocket.hpp"

namespace solid{
namespace frame{
namespace ipc{


Session::Session(
	const size_t _idx,
	const SocketDevice &_rsd
){}

Session::Session(
	const size_t _idx
){
	
}

Session::~Session(){
	
}

inline Service& Session::service(frame::aio::ReactorContext &_rctx){
	return static_cast<Service&>(_rctx.service());
}

bool Session::pushMessage(
	MessagePointerT &_rmsgptr,
	const ConnectionUid	&_rconuid_in,
	ConnectionUid *_pconuid_out,
	ulong _flags
){
	return false;
}

/*virtual*/ void Session::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	
}

/*static*/ void Session::onRecv(frame::aio::ReactorContext &_rctx, size_t _sz){
	
}
/*static*/ void Session::onSend(frame::aio::ReactorContext &_rctx){
	
}

}//namespace ipc
}//namespace frame
}//namespace solid
