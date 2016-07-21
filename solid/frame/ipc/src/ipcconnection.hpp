// solid/frame/ipc/src/ipcconnection.hpp
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

#include "solid/system/socketdevice.hpp"

#include "solid/utility/queue.hpp"
#include "solid/utility/event.hpp"
#include "solid/utility/any.hpp"

#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/frame/aio/aiostream.hpp"
#include "solid/frame/aio/aiosocket.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/frame/aio/openssl/aiosecuresocket.hpp"

#include "solid/frame/ipc/ipcconfiguration.hpp"

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


using MessageIdVectorT = std::vector<MessageId>;

class Connection: public Dynamic<Connection, frame::aio::Object>{
public:
	
	static Event eventResolve();
	static Event eventNewMessage();
	static Event eventNewMessage(const MessageId &);
	static Event eventCancelConnMessage(const MessageId &);
	static Event eventCancelPoolMessage(const MessageId &);
	static Event eventStopping();
	static Event eventEnterActive(ConnectionEnterActiveCompleteFunctionT &&, const size_t _send_buffer_capacity);
	static Event eventEnterPassive(ConnectionEnterPassiveCompleteFunctionT &&);
	static Event eventStartSecure(ConnectionSecureHandhakeCompleteFunctionT &&);
	static Event eventSendRaw(ConnectionSendRawDataCompleteFunctionT &&, std::string &&);
	static Event eventRecvRaw(ConnectionRecvRawDataCompleteFunctionT &&);
	
	//Called when connection is 
	Connection(
		Configuration const& _rconfiguration,
		ConnectionPoolId const &_rpool_id,
		std::string const & _rpool_name
	);
	
	virtual ~Connection();
	
	//NOTE: will always accept null message
	bool tryPushMessage(
		Configuration const& _rconfiguration,
		MessageBundle &_rmsgbundle,
		MessageId &_rconn_msg_id,
		const MessageId &_rpool_msg_id
	);
	
	const ErrorConditionT& error()const;
	const ErrorCodeT& systemError()const;
	
	bool isFull(Configuration const& _rconfiguration)const;
	
	bool isInPoolWaitingQueue() const;
	
	void setInPoolWaitingQueue();
	
	bool isServer()const;
	bool isConnected()const;
	bool isSecured()const;
	
	bool isWriterEmpty()const;
	
	Any<>& any();
	
	ConnectionPoolId const& poolId()const;
	const std::string& poolName()const;
	
	MessageIdVectorT const& pendingMessageVector()const{
		return pending_message_vec;
	}
	
	void pendingMessageVectorEraseFirst(const size_t _count){
		pending_message_vec.erase(pending_message_vec.begin(), pending_message_vec.begin() + _count);
	}
	
	template <class Fnc>
	void forEveryMessagesNewerToOlder(
		Fnc const &_f
	){
		auto 							visit_fnc = [this, &_f](
			MessageBundle &_rmsgbundle,
			MessageId const &_rmsgid
		){
			_f(this->poolId(), _rmsgbundle, _rmsgid);
		};
		MessageWriter::VisitFunctionT		fnc(std::cref(visit_fnc));
		
		msg_writer.forEveryMessagesNewerToOlder(fnc);
	}
	
	static void onSendAllRaw(frame::aio::ReactorContext &_rctx, Event &_revent);
	static void onRecvSomeRaw(frame::aio::ReactorContext &_rctx, const size_t _sz, Event &_revent);
protected:
	static void onRecv(frame::aio::ReactorContext &_rctx, size_t _sz);
	static void onSend(frame::aio::ReactorContext &_rctx);
	static void onConnect(frame::aio::ReactorContext &_rctx);
	static void onTimerInactivity(frame::aio::ReactorContext &_rctx);
	static void onTimerKeepalive(frame::aio::ReactorContext &_rctx);

private:
	friend struct ConnectionContext;
	friend class Service;
	
	Service& service(frame::aio::ReactorContext &_rctx)const;
	ObjectIdT uid(frame::aio::ReactorContext &_rctx)const;
	
	void onEvent(frame::aio::ReactorContext &_rctx, Event &&_uevent) override;
	
	
	bool shouldSendKeepalive()const;
	bool shouldPollPool()const;
	
	bool willAcceptNewMessage(frame::aio::ReactorContext &_rctx)const;
	
	bool isWaitingKeepAliveTimer()const;
	bool isStopPeer()const;
	
	//The connection is aware that it is activated
	bool isActiveState()const;
	bool isRawState()const;
	
	bool isStopping()const;
	bool isDelayedStopping()const;
	
	bool hasCompletingMessages()const;
	
	void onStopped(frame::aio::ReactorContext &_rctx);
	
	void doStart(frame::aio::ReactorContext &_rctx, const bool _is_incomming);
	
	void doStop(frame::aio::ReactorContext &_rctx, const ErrorConditionT &_rerr, const ErrorCodeT &_rsyserr = ErrorCodeT());
	
	void doSend(frame::aio::ReactorContext &_rctx);
	
// 	SocketDevice const & device()const{
// 		return sock.device();
// 	}
	
	void doActivate(
		frame::aio::ReactorContext &_rctx,
		Event &_revent
	);
	
	void doOptimizeRecvBuffer();
	void doOptimizeRecvBufferForced();
	void doPrepare(frame::aio::ReactorContext &_rctx);
	void doUnprepare(frame::aio::ReactorContext &_rctx);
	void doResetTimerStart(frame::aio::ReactorContext &_rctx);
	void doResetTimerSend(frame::aio::ReactorContext &_rctx);
	void doResetTimerRecv(frame::aio::ReactorContext &_rctx);
	
	void doCompleteMessage(
		frame::aio::ReactorContext &_rctx, MessagePointerT &_rresponse_ptr, const size_t _response_type_id
	);
	
	void doCompleteMessage(
		solid::frame::aio::ReactorContext& _rctx,
		MessageId const &_rpool_msg_id,
		MessageBundle &_rmsg_local,
		ErrorConditionT const &_rerr
	);
	
	void doCompleteKeepalive(frame::aio::ReactorContext &_rctx);

	void doHandleEventKill(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventStart(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventResolve(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventNewPoolMessage(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventNewConnMessage(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventCancelConnMessage(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventCancelPoolMessage(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventEnterActive(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventEnterPassive(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventStartSecure(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventSendRaw(frame::aio::ReactorContext &_rctx, Event &_revent);
	void doHandleEventRecvRaw(frame::aio::ReactorContext &_rctx, Event &_revent);
	
	void doContinueStopping(
		frame::aio::ReactorContext &_rctx,
		const Event &_revent
	);
	
	void doCompleteAllMessages(
		frame::aio::ReactorContext &_rctx,
		size_t _offset,
		const bool _can_stop,
		const ulong _seconds_to_wait,
		ErrorConditionT const &_rerr,
		Event &_revent
	);
	
private:
	virtual bool postSendAll(frame::aio::ReactorContext &_rctx, const char *_pbuf, size_t _bufcp, Event &_revent) = 0;
	virtual bool postRecvSome(frame::aio::ReactorContext &_rctx, char *_pbuf, size_t _bufcp) = 0;
	virtual bool postRecvSome(frame::aio::ReactorContext &_rctx, char *_pbuf, size_t _bufcp, Event &_revent) = 0;
	virtual bool hasValidSocket() const = 0;
	virtual bool connect(frame::aio::ReactorContext &_rctx, const SocketAddressInet&_raddr) = 0;
	virtual bool recvSome(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp, size_t &_sz) = 0;
	virtual bool hasPendingSend() const = 0;
	virtual bool sendAll(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp) = 0;
	virtual void prepareSocket(frame::aio::ReactorContext &_rctx) = 0;
	
	uint32_t recvBufferCapacity()const{
		return recv_buf_cp_kb * 1024;
	}
	uint32_t sendBufferCapacity()const{
		return send_buf_cp_kb * 1024;
	}
protected:
	typedef frame::aio::Timer				TimerT;
	
	ConnectionPoolId			pool_id;
	const std::string			&rpool_name;
	
	
	TimerT						timer;
	
	uint16_t					flags;
	
	uint32_t					recv_buf_off;
	uint32_t					cons_buf_off;
	
	uint32_t					recv_keepalive_count;
	
	char						*recv_buf;
	char						*send_buf;
	
	uint8_t						recv_buf_cp_kb;//kilobytes
	uint8_t						send_buf_cp_kb;//kilobytes
	
	MessageIdVectorT			pending_message_vec;
	
	MessageReader				msg_reader;
	MessageWriter				msg_writer;
	
	ErrorConditionT				err;
	ErrorCodeT					syserr;
	
	Any<>						any_data;
};

inline Any<>& Connection::any(){
	return any_data;
}

inline ConnectionPoolId const& Connection::poolId()const{
	return pool_id;
}

inline const std::string& Connection::poolName()const{
	return rpool_name;
}

inline bool Connection::isWriterEmpty()const{
	return msg_writer.empty();
}

inline const ErrorConditionT& Connection::error()const{
	return err;
}
inline const ErrorCodeT& Connection::systemError()const{
	return syserr;
}

//-----------------------------------------------------------------------------
//		PlainConnection
//-----------------------------------------------------------------------------

struct PlainConnection: Connection{
public:
	PlainConnection(
		Configuration const& _rconfiguration,
		SocketDevice &_rsd,
		ConnectionPoolId const &_rpool_id,
		std::string const & _rpool_name
	);
	//Called when connection is 
	PlainConnection(
		Configuration const& _rconfiguration,
		ConnectionPoolId const &_rpool_id,
		std::string const & _rpool_name
	);
	
private:
	/*virtual*/ bool postSendAll(frame::aio::ReactorContext &_rctx, const char *_pbuf, size_t _bufcp, Event &_revent) override;
	/*virtual*/ bool postRecvSome(frame::aio::ReactorContext &_rctx, char *_pbuf, size_t _bufcp, Event &_revent) override;
	/*virtual*/ bool postRecvSome(frame::aio::ReactorContext &_rctx, char *_pbuf, size_t _bufcp) override;
	/*virtual*/ bool hasValidSocket() const override;
	/*virtual*/ bool connect(frame::aio::ReactorContext &_rctx, const SocketAddressInet&_raddr) override;
	/*virtual*/ bool recvSome(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp, size_t &_sz) override;
	/*virtual*/ bool hasPendingSend() const override;
	/*virtual*/ bool sendAll(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp) override;
	/*virtual*/ void prepareSocket(frame::aio::ReactorContext &_rctx) override;
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
		SocketDevice &_rsd,
		ConnectionPoolId const &_rpool_id,
		std::string const & _rpool_name
	);
	SecureConnection(
		Configuration const& _rconfiguration,
		ConnectionPoolId const &_rpool_id,
		std::string const & _rpool_name
	);
private:
	/*virtual*/ bool postSendAll(frame::aio::ReactorContext &_rctx, const char *_pbuf, size_t _bufcp, Event &_revent) override;
	/*virtual*/ bool postRecvSome(frame::aio::ReactorContext &_rctx, char *_pbuf, size_t _bufcp, Event &_revent) override;
	/*virtual*/ bool postRecvSome(frame::aio::ReactorContext &_rctx, char *_pbuf, size_t _bufcp) override;
	/*virtual*/ bool hasValidSocket() const override;
	/*virtual*/ bool connect(frame::aio::ReactorContext &_rctx, const SocketAddressInet&_raddr) override;
	/*virtual*/ bool recvSome(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp, size_t &_sz) override;
	/*virtual*/ bool hasPendingSend() const override;
	/*virtual*/ bool sendAll(frame::aio::ReactorContext &_rctx, char *_buf, size_t _bufcp) override;
	/*virtual*/ void prepareSocket(frame::aio::ReactorContext &_rctx) override;
private:
	using StreamSocketT = frame::aio::Stream<frame::aio::openssl::Socket>;
	
	StreamSocketT				sock;
};

inline Connection * new_connection(
	Configuration const& _rconfiguration,
	SocketDevice &_rsd,
	ConnectionPoolId const &_rpool_id,
	std::string const & _rpool_name
){
	if(_rconfiguration.hasSecureContext()){
		return new SecureConnection(_rconfiguration, _rsd, _rpool_id, _rpool_name);
	}else{
		return new PlainConnection(_rconfiguration, _rsd, _rpool_id, _rpool_name);
	}
}

inline Connection * new_connection(
	Configuration const& _rconfiguration,
	ConnectionPoolId const &_rpool_id,
	std::string const & _rpool_name
){
	if(_rconfiguration.hasSecureContext()){
		return new SecureConnection(_rconfiguration, _rpool_id, _rpool_name);
	}else{
		return new PlainConnection(_rconfiguration, _rpool_id, _rpool_name);
	}
}


}//namespace ipc
}//namespace frame
}//namespace solid

#endif
