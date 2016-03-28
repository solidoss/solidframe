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

#include "frame/aio/openssl/aiosecuresocket.hpp"

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
	
	bool empty()const{
		return addrvec.empty();
	}
	
	SocketAddressInet const & currentAddress()const{
		return addrvec.back();
	}
	
	ResolveMessage(AddressVectorT &&_raddrvec):addrvec(std::move(_raddrvec)){}
	
	ResolveMessage(const ResolveMessage&) = delete;
	
	ResolveMessage(ResolveMessage &&_urm):addrvec(std::move(_urm.addrvec)){
	}	
};


class Connection: public Dynamic<Connection, frame::aio::Object>{
public:
	
	static Event eventResolve();
	static Event eventCheckPool();
	static Event eventNewMessage();
	static Event eventNewMessage(const MessageId &);
	static Event eventCancelLocalMessage(const MessageId &);
	static Event eventCancelPoolMessage(const MessageId &);
	static Event eventStopping();
	static Event eventEnterActive(ConnectionEnterActiveCompleteFunctionT &&);
	static Event eventEnterPassive(ConnectionEnterPassiveCompleteFunctionT &&);
	static Event eventStartSecure(ConnectionSecureHandhakeCompleteFunctionT &&);
	static Event eventSendRaw(ConnectionSendRawDataCompleteFunctionT &&, std::string &&);
	static Event eventRecvRaw(ConnectionRecvRawDataCompleteFunctionT &&);
	
	//Called when connection is 
	Connection(
		Configuration const& _rconfiguration,
		ConnectionPoolId const &_rconpoolid
	);
	
	virtual ~Connection();
	
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
	
	bool isFull(Configuration const& _rconfiguration)const;
	
	bool prepareActivate(
		Service &_rservice,
		ConnectionPoolId const &_rconpoolid, Event &_revent, ErrorConditionT &_rerror
	);
	
	
	bool isInPoolWaitingQueue() const;
	
	void setInPoolWaitingQueue();
	
	bool isServer()const;
	
	Any<>& any();
	
	ConnectionPoolId const& poolId()const;
	
	virtual bool postRecvSome(frame::aio::ReactorContext &_rctx, char *_pbuf, size_t _bufcp) = 0;
	virtual bool hasValidSocket()const = 0;
	virtual bool connect(frame::aio::ReactorContext &_rctx, const SocketAddressInet&_raddr) = 0;
	virtual bool recvSome(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp, size_t &_sz) = 0;
	virtual bool hasPendingSend()const = 0;
	virtual bool sendAll(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp) = 0;
protected:
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
	
	bool isStopping()const;
	bool isDelayedClosing()const;
	
	bool hasCompletingMessages()const;
	
	void onStopped(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr);
	
	void doStart(frame::aio::ReactorContext &_rctx, const bool _is_incomming);
	
	void doStop(frame::aio::ReactorContext &_rctx, ErrorConditionT const &_rerr);
	
	void doSend(frame::aio::ReactorContext &_rctx, const bool _sent_something = false);
	
// 	SocketDevice const & device()const{
// 		return sock.device();
// 	}
	
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
	void forEveryMessagesNewerToOlder(
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
protected:
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
	
	TimerT						timer;
	
	uint16						flags;
	
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

//-----------------------------------------------------------------------------
//		PlainConnection
//-----------------------------------------------------------------------------

struct PlainConnection: Connection{
public:
	PlainConnection(
		Configuration const& _rconfiguration,
		SocketDevice &_rsd, ConnectionPoolId const &_rconpoolid
	);
	//Called when connection is 
	PlainConnection(
		Configuration const& _rconfiguration,
		ConnectionPoolId const &_rconpoolid
	);
	
private:
	/*virtual*/ bool postRecvSome(frame::aio::ReactorContext &_rctx, char *_pbuf, size_t _bufcp) override;
	/*virtual*/ bool hasValidSocket()const override;
	/*virtual*/ bool connect(frame::aio::ReactorContext &_rctx, const SocketAddressInet&_raddr) override;
	/*virtual*/ bool recvSome(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp, size_t &_sz) override;
	/*virtual*/ bool hasPendingSend()const override;
	/*virtual*/ bool sendAll(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp) override;
private:
	using StreamSocketT = frame::aio::Stream<frame::aio::Socket>;
	
	StreamSocketT				sock;
};

//-----------------------------------------------------------------------------
//		SecureConnection
//-----------------------------------------------------------------------------

struct SecureConnection: Connection{
public:
	SecureConnection(
		Configuration const& _rconfiguration,
		SocketDevice &_rsd, ConnectionPoolId const &_rconpoolid
	);
	SecureConnection(
		Configuration const& _rconfiguration,
		ConnectionPoolId const &_rconpoolid
	);
private:
	/*virtual*/ bool postRecvSome(frame::aio::ReactorContext &_rctx, char *_pbuf, size_t _bufcp) override;
	/*virtual*/ bool hasValidSocket()const override;
	/*virtual*/ bool connect(frame::aio::ReactorContext &_rctx, const SocketAddressInet&_raddr) override;
	/*virtual*/ bool recvSome(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp, size_t &_sz) override;
	/*virtual*/ bool hasPendingSend()const override;
	/*virtual*/ bool sendAll(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp) override;
private:
	using StreamSocketT = frame::aio::Stream<frame::aio::openssl::Socket>;
	
	StreamSocketT				sock;
};


}//namespace ipc
}//namespace frame
}//namespace solid

#endif
