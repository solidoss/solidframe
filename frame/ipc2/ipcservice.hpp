// frame/ipc/ipcservice.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_IPCSERVICE_HPP
#define SOLID_FRAME_IPC_IPCSERVICE_HPP

#include "serialization/idtypemapper.hpp"
#include "serialization/binary.hpp"

#include "frame/service.hpp"
#include "frame/aio/aioselector.hpp"
#include "frame/scheduler.hpp"
#include "frame/ipc2/ipcsessionuid.hpp"
#include "frame/ipc2/ipcmessage.hpp"

namespace solid{

struct SocketAddressStub;
struct SocketDevice;
struct ResolveIterator;


namespace frame{

namespace aio{
class Object;

namespace openssl{
class Context;
}

}

namespace ipc{
struct PacketContext;
class Service;

enum {
	SameConnectorFlag = 1,							//!< Do not send message to a restarted peer process
	ResponseFlag	= SameConnectorFlag,			//!< The sent message is a response
	WaitResponseFlag = 2,
	SynchronousSendFlag = 4,						//!< Make the message synchronous
	CompressedSendFlag = 8,
	SecureSendFlag = 16,
	DisconnectAfterSendFlag = 32,					//!< Disconnect the session after sending the message
	
};

enum ErrorE{
	NoError = 0,
	GenericError = -1,
	NoGatewayError = -100,
	UnsupportedSocketFamilyError = -101,
	NoConnectionError = -102,
	TryReconnectError = -103,
	
};

struct Controller: Dynamic<Controller, DynamicShared<> >{
	virtual ~Controller();

	virtual void scheduleListener(frame::aio::Object *_plis) = 0;
	virtual void scheduleConnection(frame::aio::Object *_pcon) = 0;
	
	virtual bool compressPacket(
		PacketContext &_rpc,
		const uint32 _bufsz,
		char* &_rpb,
		uint32 &_bl
	);
	virtual bool decompressPacket(
		PacketContext &_rpc,
		char* &_rpb,
		uint32 &_bl
	);
	
	virtual bool onMessageReceive(
		DynamicPointer<Message> &_rmsgptr,
		ConnectionContext const &_rctx
	);
	
	virtual Message::UInt32PairT onMessagePrepare(
		Message &_rmsg,
		ConnectionContext const &_rctx
	);
	
	virtual void onMessageComplete(
		Message &_rmsg,
		ConnectionContext const &_rctx,
		int _error
	);
	
	const uint32 reservedDataSize()const{
		return resdatasz;
	}
	
	virtual void onDisconnect(const SocketAddressInet &_raddr);

protected:
	Controller(
		const uint32 _flags = 0,
		const uint32 _resdatasz = 0
	):flags(0), resdatasz(_resdatasz){}
	
private:
	uint32		flags;
	uint32		resdatasz;
};

typedef frame::Scheduler<frame::aio::Selector> AioSchedulerT;

struct BasicController: Controller{
    BasicController(
		AioSchedulerT &_rsched,
		const uint32 _flags = 0,
		const uint32 _resdatasz = 0
	);
	BasicController(
		AioSchedulerT &_rsched_l,
		AioSchedulerT &_rsched_c,
		const uint32 _flags = 0,
		const uint32 _resdatasz = 0
	);
	~BasicController();
	/*virtual*/ void scheduleListener(frame::aio::Object *_plis);
	/*virtual*/ void scheduleConnection(frame::aio::Object *_pcon);
private:
	AioSchedulerT &rsched_l;
	AioSchedulerT &rsched_c;
};


struct Configuration{
	bool operator==(const Configuration &_rcfg)const{
		return	false;
	}
	//TODO:...
};

//! An Inter Process Communication service
/*!
	Allow for sending/receiving serializable ipc::Messages between processes.
	A process is identified by a pair [IP address, port].<br>
	<br>
*/
class Service: public Dynamic<Service, frame::Service>{
public:
	typedef serialization::binary::Serializer<
		const ConnectionContext
	>						SerializerT;
	typedef serialization::binary::Deserializer<
		const ConnectionContext
	>						DeserializerT;
private:
	typedef serialization::IdTypeMapper<
		SerializerT,
		DeserializerT,
		SerializationTypeIdT
	>						IdTypeMapperT;
	
	struct Handle{
		bool checkStore(void */*_pt*/, const ConnectionContext &/*_rctx*/)const{
			return true;
		}
		bool checkLoad(void */*_pt*/, const ConnectionContext &/*_rctx*/)const{
			return true;
		}
		void afterSerialization(SerializerT &_rs, void *_pm, const ConnectionContext &_rctx){}
		void afterSerialization(DeserializerT &_rs, void *_pm, const ConnectionContext &_rctx){}
	
		void beforeSerialization(SerializerT &_rs, Message *_pt, const ConnectionContext &_rctx);
		void beforeSerialization(DeserializerT &_rs, Message *_pt, const ConnectionContext &_rctx);
	};
	
	template <class H>
	struct ProxyHandle: H, Handle{
		void beforeSerialization(SerializerT &_rs, Message *_pt, const ConnectionContext &_rctx){
			this->Handle::beforeSerialization(_rs, _pt, _rctx);
		}
		void beforeSerialization(DeserializerT &_rd, Message *_pt, const ConnectionContext &_rctx){
			this->Handle::beforeSerialization(_rd, _pt, _rctx);
		}
	};
	
public:
	typedef Dynamic<Service, frame::Service> BaseT;
	
	static const char* errorText(int _err);
	
	Service(
		frame::Manager &_rm,
		const DynamicPointer<Controller> &_rctrlptr
	);
	
	template <class T>
	uint32 registerMessageType(uint32 _idx = 0){
		return typeMapper().insertHandle<T, Handle>(_idx);
	}
	
	template <class T, class H>
	uint32 registerMessageType(uint32 _idx = 0){
		return typeMapper().insertHandle<T, ProxyHandle<Handle> >(_idx);
	}
	
	//! Destructor
	~Service();

	bool reconfigure(Configuration const& _rcfg);
	
	int listenPort()const;
	
	//!Send a message (usually a response) to a peer process using a previously saved ConnectionUid
	/*!
		The message is send only if the connector exists. If the peer process,
		restarts the message is not sent.
		\param _rmsgptr A DynamicPointer with the message to be sent.
		\param _rssnid A previously saved SessionUid
		\param _flags Control flags
	*/
	
	bool sendMessage(
		DynamicPointer<Message> &_rmsgptr,//the message to be sent
		const SessionUid &_rssnid,//the id of the process session
		uint32	_flags = 0
	);
	
	//!Send a message to a peer process using it's base address.
	/*!
		The base address of a process is the address on which the process listens for new UDP connections.
		If the connection does not already exist, it will be created.
		\param _rmsgptr The message.
		\param _rssnid An output value, which on success will contain the uid of the connector.
		\param _rsa_dest Destination address
		\param _flags Control flags
	*/
	bool sendMessage(
		DynamicPointer<Message> &_rmsgptr,//the message to be sent
		SessionUid &_rssnid,
		const char *_addr_cstr,
		int _port = -1,
		uint32	_flags = 0
	);
	//!Send a message to a peer process using it's base address.
	/*!
		The base address of a process is the address on which the process listens for new UDP connections.
		If the connection does not already exist, it will be created.
		\param _rmsgptr The message.
		\param _rsa_dest Destination address
		\param _flags Control flags
	*/
	bool sendMessage(
		DynamicPointer<Message> &_rmsgptr,//the message to be sent
		const char *_addr_cstr,
		int _port = -1,
		uint32	_flags = 0
	);
	
	///////
	bool sendMessage(
		DynamicPointer<Message> &_rmsgptr,//the message to be sent
		const SerializationTypeIdT &_rtid,
		const SessionUid &_rssnid,
		uint32	_flags = 0
	);
	
	//!Send a message to a peer process using it's base address.
	/*!
		The base address of a process is the address on which the process listens for new UDP connections.
		If the connection does not already exist, it will be created.
		\param _rmsgptr The message.
		\param _rssnid An output value, which on success will contain the uid of the connector.
		\param _rsa_dest Destination address
		\param _flags Control flags
	*/
	bool sendMessage(
		DynamicPointer<Message> &_rmsgptr,//the message to be sent
		const SerializationTypeIdT &_rtid,
		SessionUid &_rssnid,
		const char *_addr_cstr,
		int _port = -1,
		uint32	_flags = 0
	);
	//!Send a message to a peer process using it's base address.
	/*!
		The base address of a process is the address on which the process listens for new UDP connections.
		If the connection does not already exist, it will be created.
		\param _rmsgptr The message.
		\param _rsa_dest Destination address
		\param _flags Control flags
	*/
	bool sendMessage(
		DynamicPointer<Message> &_rmsgptr,//the message to be sent
		const SerializationTypeIdT &_rtid,
		const char *_addr_cstr,
		int _port = -1,
		uint32	_flags = 0
	);
	
	serialization::TypeMapperBase const& typeMapperBase() const;
	Configuration const& configuration()const;
private:
	friend class Connection;
	friend class Controller;
	friend class Listener;
	
	IdTypeMapperT& typeMapper();
	
	bool doSendMessage(
		DynamicPointer<Message> &_rmsgptr,//the message to be sent
		const SerializationTypeIdT &_rtid,
		SessionUid *_psesid,
		const char *_addr_cstr,
		int _port = -1,
		uint32	_flags = 0
	);
	
	void insertConnection(
		SocketDevice &_rsd,
		aio::openssl::Context *_pctx
	);
	
	uint32 keepAliveTimeout()const;
	
	Controller& controller();
	
	Controller const& controller()const;
private:
	struct Data;
	friend struct Data;
	Data			&d;
	IdTypeMapperT	typemapper;
};


inline bool Service::sendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	SessionUid &_rsesid,
	const char *_addr_cstr,
	int _port,
	uint32	_flags
){
	return doSendMessage(_rmsgptr, SERIALIZATION_INVALIDID, &_rsesid, _rsa_dest, _flags);
}

inline bool Service::sendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const char *_addr_cstr,
	int _port,
	uint32	_flags
){
	return doSendMessage(_rmsgptr, SERIALIZATION_INVALIDID, NULL, _addr_cstr, _port, _flags);
}

inline bool Service::sendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SerializationTypeIdT &_rtid,
	SessionUid &_rsesid,
	const char *_addr_cstr,
	int _port,
	uint32	_flags
){
	return doSendMessage(_rmsgptr, _rtid, &_rsesid, _addr_cstr, _port, _flags);
}

inline bool Service::sendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SerializationTypeIdT &_rtid,
	const char *_addr_cstr,
	int _port,
	uint32	_flags
){
	return doSendMessage(_rmsgptr, _rtid, NULL, _addr_cstr, _port, _flags);
}

inline const serialization::TypeMapperBase& Service::typeMapperBase() const{
	return typemapper;
}

inline Service::IdTypeMapperT& Service::typeMapper(){
	return typemapper;
}



}//namespace ipc
}//namespace frame
}//namespace solid

#endif
