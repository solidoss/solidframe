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

#include "system/socketdevice.hpp"

#include "utility/queue.hpp"

#include "frame/aio/aioobject.hpp"
#include "frame/aio/aioreactorcontext.hpp"
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
	
	struct MessageStub{
		MessageStub(
			MessagePointerT &_rmsgptr,
			ulong _flags
		): msgptr(std::move(_rmsgptr)), flags(_flags){}
		MessagePointerT msgptr;
		ulong			flags;
	};
	
	typedef Queue<MessageStub>							MessageQueueT;
	
	struct ConnectionStub{
		ConnectionStub(aio::ObjectProxy const &_robj):sock(_robj), timer(_robj), unique(0){}
		ConnectionStub(
			ConnectionStub && _rcs
		):sock(std::move(_rcs.sock)), timer(std::move(_rcs.timer)), msgq(std::move(_rcs.msgq)), unique(_rcs.unique){}
		
		StreamSocketT	sock;
		TimerT			timer;
		MessageQueueT	msgq;
		uint16			unique;
	};
	typedef std::vector<ConnectionStub>					ConnectionVectorT;
	struct IncommingMessageStub{
		IncommingMessageStub(
			MessagePointerT &_rmsgptr,
			UidT const& _rconuid,
			ulong _flags
		): msgptr(_rmsgptr), conuid(_rconuid), flags(_flags){}
		
		MessagePointerT		msgptr;
		UidT				conuid;
		ulong				flags;
	};
	
	typedef std::vector<IncommingMessageStub>			IncommingMessageVectorT;
	
	size_t					idx;
	uint16					crtpushvecidx;
	ConnectionVectorT		convec;
	IncommingMessageVectorT	incommingmsgvec[2];
	MessageQueueT			msgq;
};


}//namespace ipc
}//namespace frame
}//namespace solid

#endif
