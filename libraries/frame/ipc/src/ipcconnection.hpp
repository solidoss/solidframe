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
#include "utility/event.hpp"
#include "utility/any.hpp"

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


struct ResolveMessage{
	AddressVectorT	addrvec;
	size_t			crtidx;
	
	SocketAddressInet const & currentAddress()const{
		return addrvec[crtidx];
	}
	
	ResolveMessage(AddressVectorT &_raddrvec):addrvec(std::move(_raddrvec)), crtidx(0){}
	
	ResolveMessage(const ResolveMessage&) = delete;
	
	ResolveMessage(ResolveMessage &&_urm):addrvec(std::move(_urm.addrvec)), crtidx(_urm.crtidx){
		_urm.crtidx = 0;
	}
	
};


class Connection: public Dynamic<Connection, frame::aio::Object>{
public:
	
	static Event resolveEvent();
	static Event checkPoolEvent();
	static Event newMessageEvent();
	static Event newMessageEvent(const MessageId &);
	static Event cancelLocalMessageEvent(const MessageId &);
	static Event cancelPoolMessageEvent(const MessageId &);
	static Event activateEvent(ActivateConnectionMessageFactoryFunctionT &&);
	
	//Called when connection is accepted
	Connection(
		SocketDevice &_rsd, ConnectionPoolId const &_rconpoolid
	);
	//Called when connection is 
	Connection(
		ConnectionPoolId const &_rconpoolid
	);
	~Connection();
	
	//We cannot use MessageBundle, because we do not
	//want to store MessagePointerT and _rresponse_fnc
	//in a temporary MessageBundle
	bool pushMessage(
		Service &_rservice,
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ResponseHandlerFunctionT &_rresponse_fnc,
		ulong _flags,
		MessageId *_pmsguid,
		Event &_revent,
		ErrorConditionT &_rerror,
		const MessageId &_rpool_msg_id
	);
	
	bool pushCanceledMessage(
		Service &_rservice,
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ResponseHandlerFunctionT &_rresponse_fnc,
		ulong _flags,
		Event &_revent,
		ErrorConditionT &_rerror,
		const MessageId &_rpool_msg_id
	);
	
	bool pushMessageCancel(
		Service &_rservice,
		MessageId const &_rmsguid,
		Event &_revent,
		ErrorConditionT &_rerror
	);
	
	bool pushDelayedClose(
		Service &_rservice,
		Event &_revent,
		ErrorConditionT &_rerror
	);
	//NOTE: will always accept null message
	bool tryPushMessage(
		MessageBundle &_rmsgbundle,
		MessageId &_rmsguid,
		const MessageId &_rpool_msg_id
	);
	
	//NOTE: will always accept null message
	bool tryPushMessage(
		MessageBundle &_rmsgbundle
	);
	
	bool prepareActivate(
		Service &_rservice,
		ConnectionPoolId const &_rconpoolid, Event &_revent, ErrorConditionT &_rerror
	);
	
	//The service marked connection as active, but the connection might not be aware that it is active
	bool isAtomicActive()const;
	
	bool isInPoolWaitingQueue() const;
	
	void setInPoolWaitingQueue();
	
	bool isServer()const;
	
	Any<>& any();
	
	ConnectionPoolId const& poolId()const;
private:
	friend struct ConnectionContext;
	friend class Service;
	
	Service& service(frame::aio::ReactorContext &_rctx)const;
	ObjectIdT uid(frame::aio::ReactorContext &_rctx)const;
	
	void onEvent(frame::aio::ReactorContext &_rctx, Event &&_uevent) override;
	
	static void onRecv(frame::aio::ReactorContext &_rctx, size_t _sz);
	static void onSend(frame::aio::ReactorContext &_rctx);
	static void onConnect(frame::aio::ReactorContext &_rctx);
	static void onTimerInactivity(frame::aio::ReactorContext &_rctx);
	static void onTimerKeepalive(frame::aio::ReactorContext &_rctx);
	static void onTimerWaitStopping(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr);
	
	
	bool shouldSendKeepalive()const;
	bool shouldTryFetchNewMessageFromPool()const;
	
	bool isWaitingKeepAliveTimer()const;
	bool isStopForced()const;
	
	//The connection is aware that it is activated
	bool isActive()const;
	
	bool isAtomicStopping()const;
	bool isAtomicDelayedClosing()const;
	
	bool hasCompletingMessages()const;
	
	void onStopped(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr);
	
	void doStart(frame::aio::ReactorContext &_rctx, const bool _is_incomming);
	
	void doStop(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr);
	
	void doSend(frame::aio::ReactorContext &_rctx, const bool _sent_something = false);
	
	SocketDevice const & device()const{
		return sock.device();
	}
	
	void doActivate(
		frame::aio::ReactorContext &_rctx,
		Event &_revent
	);
	
	void doCompleteAllMessages(
		frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr
	);
	
	void doOptimizeRecvBuffer();
	void doOptimizeRecvBufferForced();
	void doPrepare(frame::aio::ReactorContext &_rctx);
	void doUnprepare(frame::aio::ReactorContext &_rctx);
	void doResetTimerStart(frame::aio::ReactorContext &_rctx);
	void doResetTimerSend(frame::aio::ReactorContext &_rctx);
	void doResetTimerRecv(frame::aio::ReactorContext &_rctx);
	void doCompleteMessage(solid::frame::aio::ReactorContext& _rctx, solid::frame::ipc::MessagePointerT /*const*/& _rmsgptr);
	void doCompleteKeepalive(frame::aio::ReactorContext &_rctx);

	void doHandleEventKill(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventStart(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventActivate(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventPush(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventResolve(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventDelayedClose(frame::aio::ReactorContext &_rctx, Event &_revent);
	
	template <class Fnc>
	void fetchUnsentMessages(
		Fnc const &_f
	){
		auto 							visit_fnc = [this, &_f](
			MessageBundle &_rmsgbundle,
			MessageId const &_rmsgid
		){
			_f(this->conpoolid, _rmsgbundle, _rmsgid);
		};
		MessageWriterVisitFunctionT		fnc(std::cref(visit_fnc));
		
		msgwriter.visitAllMessages(fnc);
	}
	
	template <class Fnc>
	void visitCompletingMessages(
		Fnc const &_f
	){
		MessageWriterCompletingVisitFunctionT	fnc(std::cref(_f));
		
		msgwriter.visitCompletingMessages(fnc);
	}
private:
	typedef frame::aio::Stream<frame::aio::Socket>		StreamSocketT;
	typedef frame::aio::Timer							TimerT;
	typedef std::atomic<uint8>							AtomicUInt8T;
	
	struct PendingSendMessageStub{
		PendingSendMessageStub(
			MessageBundle &&_rmsgbundle,
			MessageId const &_rmsguid
		):	msgbundle(std::move(_rmsgbundle)),
			msguid(_rmsguid)
			{}
		
		PendingSendMessageStub(
			MessageId const &_rmsguid
		):	msguid(_rmsguid)
			{}
		
		PendingSendMessageStub(
		){}
		
		
		MessageBundle				msgbundle;
		MessageId					msguid;
	};

	typedef std::vector<PendingSendMessageStub>			PendingSendMessageVectorT;
	
	ConnectionPoolId			conpoolid;
	PendingSendMessageVectorT	sendmsgvec[2];
	StreamSocketT				sock;
	TimerT						timer;
	
	uint8						crtpushvecidx;
	uint8						flags;
	AtomicUInt8T				atomic_flags;
	
	uint32						receivebufoff;
	uint32						consumebufoff;
	uint32						receive_keepalive_count;
	
	char						*recvbuf;
	char						*sendbuf;
	
	MessageReader				msgreader;
	MessageWriter				msgwriter;
	
	Any<>						any_data;
};

inline Any<>& Connection::any(){
	return any_data;
}

inline ConnectionPoolId const& Connection::poolId()const{
	return conpoolid;
}

inline bool Connection::hasCompletingMessages()const{
	return msgwriter.hasCompletingMessages();
}

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
