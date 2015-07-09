// frame/ipc/src/ipcconnection.hpp
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

#include "frame/ipc/ipcconfiguration.hpp"

#include "ipcmessagereader.hpp"
#include "ipcmessagewriter.hpp"

namespace solid{
namespace frame{
namespace aio{

namespace openssl{
class Context;
}//namespace openssl

}//namespace aio

namespace ipc{

class Service;


struct ResolveMessage: Dynamic<ResolveMessage>{
	AddressVectorT	addrvec;
	size_t			crtidx;
	
	SocketAddressInet const & currentAddress()const{
		return addrvec[crtidx];
	}
	
	ResolveMessage(AddressVectorT &_raddrvec):addrvec(std::move(_raddrvec)), crtidx(0){}
};


class Connection: public Dynamic<Connection, frame::aio::Object>{
public:
	
	static Event activateEvent(ConnectionPoolUid const& _rconpoolid);
	static Event activateEvent();
	
	//Called when connection is accepted
	Connection(
		SocketDevice &_rsd
	);
	//Called when connection is 
	Connection(
		ConnectionPoolUid const &_rconpoolid
	);
	~Connection();
	
	bool pushMessage(
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ulong _flags
	);
	
	void directPushMessage(
		frame::aio::ReactorContext &_rctx,
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ulong _flags
	);
	
	bool isActive()const;
	bool isStopping()const;
	bool isServer()const;
	bool shouldSendKeepalive()const;
	bool isWaitingKeepAliveTimer()const;
	bool isStopForced()const;
	
	HolderT& holder();
	ConnectionPoolUid const& poolUid()const;
private:
	friend struct ConnectionContext;
	friend class Service;
	
	Service& service(frame::aio::ReactorContext &_rctx)const;
	ObjectUidT uid(frame::aio::ReactorContext &_rctx)const;
	/*virtual*/ void onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	static void onRecv(frame::aio::ReactorContext &_rctx, size_t _sz);
	static void onSend(frame::aio::ReactorContext &_rctx);
	static void onConnect(frame::aio::ReactorContext &_rctx);
	static void onTimerInactivity(frame::aio::ReactorContext &_rctx);
	static void onTimerKeepalive(frame::aio::ReactorContext &_rctx);
	
	void doStart(frame::aio::ReactorContext &_rctx, const bool _is_incomming);
	
	void doStop(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr);
	
	void doSend(frame::aio::ReactorContext &_rctx, const bool _sent_something = false);
	
	SocketDevice const & device()const{
		return sock.device();
	}
	
	void doActivate(
		frame::aio::ReactorContext &_rctx,
		frame::Event const &_revent
	);
	
	void doCompleteAllMessages(
		frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr
	);
	
	void doOptimizeRecvBuffer();
	void doPrepare(frame::aio::ReactorContext &_rctx);
	void doUnprepare(frame::aio::ReactorContext &_rctx);
	void doResetTimerStart(frame::aio::ReactorContext &_rctx);
	void doResetTimerSend(frame::aio::ReactorContext &_rctx);
	void doResetTimerRecv(frame::aio::ReactorContext &_rctx);
	void doCompleteMessage(frame::aio::ReactorContext &_rctx, MessagePointerT const &_rmsgptr);
	void doCompleteKeepalive(frame::aio::ReactorContext &_rctx);
private:
	typedef frame::aio::Stream<frame::aio::Socket>		StreamSocketT;
	typedef frame::aio::Timer							TimerT;
	
	struct PendingSendMessageStub{
		PendingSendMessageStub(
			MessagePointerT &_rmsgptr,
			const size_t _msg_type_idx,
			ulong _flags
		): msgptr(_rmsgptr), msg_type_idx(_msg_type_idx), flags(_flags){}
		
		MessagePointerT		msgptr;
		const size_t		msg_type_idx;
		ulong				flags;
	};

	typedef std::vector<PendingSendMessageStub>			PendingSendMessageVectorT;
	
	ConnectionPoolUid			conpoolid;
	PendingSendMessageVectorT	sendmsgvec[2];
	StreamSocketT				sock;
	TimerT						timer;
	
	uint8						crtpushvecidx;
	uint8						flags;
	
	uint32						receivebufoff;
	uint32						consumebufoff;
	
	char						*recvbuf;
	char						*sendbuf;
	
	MessageReader				msgreader;
	MessageWriter				msgwriter;
	
	HolderT						hldr;
};

inline HolderT& Connection::holder(){
	return hldr;
}

inline ConnectionPoolUid const& Connection::poolUid()const{
	return conpoolid;
}

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
