// frame/ipc/src/ipcsession.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_SRC_IPC_CONNECTION_HPP
#define SOLID_FRAME_IPC_SRC_IPC_CONNECTION_HPP

#include "frame/aio/aioobject.hpp"
#include "frame/aio/aioreactorcontext.hpp"
#include "system/socketdevice.hpp"
#include "frame/aio/aiostream.hpp"
#include "frame/aio/aiosocket.hpp"
#include "frame/aio/aiotimer.hpp"
#include "frame/ipc/ipcmessage.hpp"
#include "frame/ipc/ipcconfiguration.hpp"

namespace solid{
namespace frame{
namespace aio{

namespace openssl{
class Context;
}//namespace openssl

}//namespace aio

namespace ipc{

class Service;
struct ConnectionUid;


struct ResolveMessage: Dynamic<ResolveMessage>{
	AddressVectorT	addrvec;
	
	ResolveMessage(AddressVectorT &_raddrvec):addrvec(std::move(_raddrvec)){}
};


class Session: public Dynamic<Session, frame::aio::Object>{
public:
	Session(
		const size_t _idx,
		const SocketDevice &_rsd
	);
	Session(
		const size_t _idx
	);
	~Session();
	
	bool pushMessage(
		MessagePointerT &_rmsgptr,
		const ConnectionUid	&_rconuid_in,
		ConnectionUid *_pconuid_out,
		ulong _flags
	);
private:
	Service& service(frame::aio::ReactorContext &_rctx);
	/*virtual*/ void onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	static void onRecv(frame::aio::ReactorContext &_rctx, size_t _sz);
	static void onSend(frame::aio::ReactorContext &_rctx);
private:
	typedef frame::aio::Stream<frame::aio::Socket>		StreamSocketT;
	typedef frame::aio::Timer							TimerT;
	struct ConnectionStub{
		ConnectionStub(ConnectionStub && _rcs):sock(std::move(_rcs.sock)), timer(std::move(_rcs.timer)){}
		
		StreamSocketT	sock;
		TimerT			timer;
	};
	
	typedef std::vector<ConnectionStub>					ConnectionVectorT;
	
	size_t					idx;
	ConnectionVectorT		convec;
};


}//namespace ipc
}//namespace frame
}//namespace solid

#endif
