// frame/ipc/ipcservice.hpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
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
#include "frame/ipc/ipcconnectionuid.hpp"
#include "frame/ipc/ipcmessage.hpp"

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

class Session;
class Talker;
class Node;
struct TalkerStub;
class Connection;
struct Packet;
struct PacketContext;
struct ConnectionUid;
struct ConnectData;
struct AcceptData;
class Service;

enum {
	SameConnectorFlag = 1, //!< Do not send message to a restarted peer process
	ResponseFlag	= SameConnectorFlag, //!< The sent message is a response
	WaitResponseFlag = 2,
	SynchronousSendFlag = 4,//!< Make the message synchronous
	SentFlag = 8,//!< The message was successfully sent
	AuthenticationFlag = 16,//!< The message is for authentication
	DisconnectAfterSendFlag = 32,//!< Disconnect the session after sending the message
};

enum Error{
	NoError = 0,
	GenericError = -1,
	NoGatewayError = -100,
	UnsupportedSocketFamilyError = -101,
	NoConnectionError = -102,
	TryReconnectError = -103,
	
};

struct Controller: Dynamic<Controller, DynamicShared<> >{
	enum{
		AuthenticationFlag = 1
	};
	
	virtual ~Controller();

	virtual void scheduleTalker(frame::aio::Object *_ptkr) = 0;
	virtual void scheduleListener(frame::aio::Object *_plis) = 0;
	virtual void scheduleNode(frame::aio::Object *_pnod) = 0;
	
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
	
	bool hasAuthentication()const{
		return (flags & AuthenticationFlag) != 0;
	}
	
	virtual bool onMessageReceive(
		DynamicPointer<Message> &_rmsgptr,
		ConnectionContext const &_rctx
	);
	
	virtual uint32 onMessagePrepare(
		Message &_rmsg,
		ConnectionContext const &_rctx
	);
	
	virtual void onMessageComplete(
		Message &_rmsg,
		ConnectionContext const &_rctx,
		int _error
	);
	
	//Must return:
	// If _psig is NULL, the request is to initiate the authentication,
	// and _psig must be filled with a command to be sent to peer.
	// * OK for authentication competed with success - session enters in AUTHENTICATED state
	// * NOK authentication not completed - session stays in AUTHENTICATED state
	// * BAD authentication completed with error - session enters Disconnecting
	virtual AsyncE authenticate(
		DynamicPointer<Message> &_rmsgptr,//the received message
		ipc::MessageUid &_rmsguid,
		uint32 &_rflags,
		SerializationTypeIdT &_rtid
	);
	const uint32 reservedDataSize()const{
		return resdatasz;
	}
	virtual uint32 computeNetworkId(
		const SocketAddressStub &_rsa_dest
	)const;
	
	virtual void onDisconnect(const SocketAddressInet &_raddr, const uint32 _netid);

protected:
	Controller(
		const uint32 _flags = 0,
		const uint32 _resdatasz = 0
	):flags(0), resdatasz(_resdatasz){}
	
	char * allocateBuffer(PacketContext &_rpc, uint32 &_cp);
	
	void sendEvent(
		Service &_rs,
		const ConnectionUid &_rconid,//the id of the process connectors
		int32 _event,
		uint32 _flags
	);
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
		AioSchedulerT &_rsched_t,
		AioSchedulerT &_rsched_l,
		AioSchedulerT &_rsched_n,
		const uint32 _flags = 0,
		const uint32 _resdatasz = 0
	);
	~BasicController();
	/*virtual*/ void scheduleTalker(frame::aio::Object *_ptkr);
	/*virtual*/ void scheduleListener(frame::aio::Object *_plis);
	/*virtual*/ void scheduleNode(frame::aio::Object *_pnod);
private:
	AioSchedulerT &rsched_t;
	AioSchedulerT &rsched_l;
	AioSchedulerT &rsched_n;
};


struct Configuration{
	struct Talker{
		Talker():sescnt(1024), maxcnt(2){}
		bool operator==(const Talker &_rtkr)const{
			return sescnt == _rtkr.sescnt && maxcnt == _rtkr.maxcnt;
		}
		uint32				sescnt;//TODO: move to parent
		uint32				maxcnt;//TODO: move to parent
	};
	struct Node{
		Node(): sescnt(1024), sockcnt(256), maxcnt(2), timeout(0){}
		bool operator==(const Node &_rnod)const{
			return sescnt == _rnod.sescnt && maxcnt == _rnod.maxcnt && sockcnt == _rnod.sockcnt && timeout == _rnod.timeout;
		}
		uint32	sescnt;
		uint32	sockcnt;
		uint32	maxcnt;
		uint32	timeout;//zero means it will be computed based on session keepalive values
	};
	struct Session{
		Session(
			const uint32 _reskeepalive = 0,
			const uint32 _seskeepalive = 60 * 1000,
			const uint32 _relayreskeepalive = 0,
			const uint32 _relayseskeepalive = 5 * 60 * 1000
		):	responsekeepalive(
			_reskeepalive < _seskeepalive 
			? 
			(_reskeepalive == 0 ? _seskeepalive : _reskeepalive )
			: 
			(_seskeepalive == 0 ? _reskeepalive : _seskeepalive)
		),
		keepalive(_seskeepalive),
		relayresponsekeepalive(
			_relayreskeepalive < _relayseskeepalive 
			? 
			(_relayreskeepalive == 0 ? _relayseskeepalive : _relayreskeepalive )
			: 
			(_relayseskeepalive == 0 ? _relayreskeepalive : _relayseskeepalive)
		),
		relaykeepalive(_relayseskeepalive),
		maxsendpacketcount(4),
		maxrecvnoupdatecount(2),
		maxmessagepacketcount(8),
		maxsendmessagequeuesize(32),
		connectretransmitcount(4),
		dataretransmitcount(4)
		{}
		bool operator==(const Session &_rses)const{
			return responsekeepalive == _rses.responsekeepalive &&
				keepalive == _rses.keepalive && maxrecvnoupdatecount == _rses.maxrecvnoupdatecount &&
				relayresponsekeepalive == _rses.relayresponsekeepalive &&
				relaykeepalive == _rses.relaykeepalive &&
				maxsendpacketcount == _rses.maxsendpacketcount &&
				maxmessagepacketcount == _rses.maxmessagepacketcount &&
				maxsendmessagequeuesize == _rses.maxsendmessagequeuesize &&
				connectretransmitcount == _rses.connectretransmitcount &&
				dataretransmitcount == _rses.dataretransmitcount;
		}
		uint32		responsekeepalive;
		uint32		keepalive;
		uint32		relayresponsekeepalive;
		uint32		relaykeepalive;
		uint32		maxsendpacketcount;
		uint32		maxrecvnoupdatecount;// max number of packets received, without sending update
		uint32		maxmessagepacketcount;//continuous packets sent for a message
		uint32		maxsendmessagequeuesize;//how many messages are sent in parallel
		uint32		connectretransmitcount;
		uint32		dataretransmitcount;
	};
	
	Configuration(
		const uint32 _flags = 0
	):	flags(_flags), localnetid(LocalNetworkId){}
	
	bool operator==(const Configuration &_rcfg)const{
		return	flags == _rcfg.flags && localnetid == _rcfg.localnetid &&
				baseaddr == _rcfg.baseaddr && talker == _rcfg.talker &&
				node == _rcfg.node && session == _rcfg.session;
	}
	
	typedef std::vector<SocketAddressInet>	SocketAddressInetVectorT;
	struct RelayAddress{
		RelayAddress():networkid(InvalidNetworkId){}
		uint32 				networkid;
		SocketAddressInet	address;
	};
	typedef std::vector<RelayAddress>		RelayAddressVectorT;
	
	uint32						flags;
	uint32						localnetid;
	SocketAddressInet			baseaddr;
	SocketAddressInet			acceptaddr;
	SocketAddressInetVectorT	gatewayaddrvec;
	RelayAddressVectorT			relayaddrvec;
	Talker						talker;
	Node						node;
	Session						session;
};

//! An Inter Process Communication service
/*!
	Allow for sending/receiving serializable foundation::Signal objects between
	processes.
	For processes within the same network, the messages are sent using a reliable
	protocol based on UDP.
	For processes within different networks, SolidFrame allows for configuring
	gateway processes which will allow tunneling the ipc messages between 
	networks over secure TCP connections.
	A process is identified by a tuple [networkid, IP address, base port].<br>
	+ networkid - the id of the network for the destination process. Default
	value is LocalNetworkId in which case no relay is used.<br>
	+ IPaddress:base port - because we want to scale up, the IPC service uses
	multiple UPD sockets to communicate between processes. One of these sockets 
	is opened on a defined port, and is the one receiving "Connect" messages.
	After a session is established between two processes, the communication may
	continue between other UDP sockets (on random ports).<br>
	
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
		
	enum Types{
		PlainType = 1,
		RelayType
	};
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
	
	TimeSpec const& timeStamp()const;
	
	int basePort()const;
	
	//!Send a message (usually a response) to a peer process using a previously saved ConnectionUid
	/*!
		The message is send only if the connector exists. If the peer process,
		restarts the message is not sent.
		\param _rmsgptr A DynamicPointer with the message to be sent.
		\param _rconid A previously saved connectionuid
		\param _flags Control flags
	*/
	
	bool sendMessage(
		DynamicPointer<Message> &_rmsgptr,//the message to be sent
		const ConnectionUid &_rconid,//the id of the process connector
		uint32	_flags = 0
	);
	
	//!Send a message to a peer process using it's base address.
	/*!
		The base address of a process is the address on which the process listens for new UDP connections.
		If the connection does not already exist, it will be created.
		\param _rsap The base socket address of the peer.
		\param _rmsgptr The message.
		\param _rconid An output value, which on success will contain the uid of the connector.
		\param _flags Control flags
	*/
	bool sendMessage(
		DynamicPointer<Message> &_rmsgptr,//the message to be sent
		ConnectionUid &_rconid,
		const SocketAddressStub &_rsa_dest,
		const uint32 _netid_dest = LocalNetworkId,
		uint32	_flags = 0
	);
	//!Send a message to a peer process using it's base address.
	/*!
		The base address of a process is the address on which the process listens for new UDP connections.
		If the connection does not already exist, it will be created.
		\param _rsap The base socket address of the peer.
		\param _rmsgptr The message.
		\param _flags Control flags
	*/
	bool sendMessage(
		DynamicPointer<Message> &_rmsgptr,//the message to be sent
		const SocketAddressStub &_rsas_dest,
		const uint32 _netid_dest = LocalNetworkId,
		uint32	_flags = 0
	);
	
	///////
	bool sendMessage(
		DynamicPointer<Message> &_rmsgptr,//the message to be sent
		const SerializationTypeIdT &_rtid,
		const ConnectionUid &_rconid,//the id of the process connector
		uint32	_flags = 0
	);
	
	//!Send a message to a peer process using it's base address.
	/*!
		The base address of a process is the address on which the process listens for new UDP connections.
		If the connection does not already exist, it will be created.
		\param _rsap The base socket address of the peer.
		\param _rmsgptr The message.
		\param _rconid An output value, which on success will contain the uid of the connector.
		\param _flags Control flags
	*/
	bool sendMessage(
		DynamicPointer<Message> &_pmsgptr,//the message to be sent
		const SerializationTypeIdT &_rtid,
		ConnectionUid &_rconid,
		const SocketAddressStub &_rsa_dest,
		const uint32 _netid_dest = LocalNetworkId,
		uint32	_flags = 0
	);
	//!Send a message to a peer process using it's base address.
	/*!
		The base address of a process is the address on which the process listens for new UDP connections.
		If the connection does not already exist, it will be created.
		\param _rsap The base socket address of the peer.
		\param _rmsgptr The message.
		\param _flags Control flags
	*/
	bool sendMessage(
		DynamicPointer<Message> &_rmsgptr,//the message to be sent
		const SerializationTypeIdT &_rtid,
		const SocketAddressStub &_rsa_dest,
		const uint32 _netid_dest = LocalNetworkId,
		uint32	_flags = 0
	);
	
	serialization::TypeMapperBase const& typeMapperBase() const;
	Configuration const& configuration()const;
private:
	friend class Talker;
	friend class Session;
	friend class Controller;
	friend class Listener;
	friend class Node;
	
	
	typedef std::vector<const Configuration::RelayAddress*>		RelayAddressPointerVectorT;
	
	IdTypeMapperT& typeMapper();
	
	bool isLocalNetwork(
		const SocketAddressStub &_rsa_dest,
		const uint32 _netid_dest
	)const;
	
	
	uint32 computeNetworkId(
		const SocketAddressStub &_rsa_dest,
		const uint32 _netid_dest
	)const;
	
	bool isGateway()const;
	
	void doSendEvent(
		const ConnectionUid &_rconid,//the id of the process connectors
		int32 _event,
		uint32 _flags = 0
	);
	
	bool doSendMessage(
		DynamicPointer<Message> &_rmsgptr,//the message to be sent
		const SerializationTypeIdT &_rtid,
		ConnectionUid *_pconid,
		const SocketAddressStub &_rsap,
		const uint32 _netid_dest,
		uint32	_flags = 0
	);
	
	bool doSendMessageLocal(
		DynamicPointer<Message> &_rmsgptr,//the message to be sent
		const SerializationTypeIdT &_rtid,
		ConnectionUid *_pconid,
		const SocketAddressStub &_rsap,
		const uint32 _netid_dest,
		uint32	_flags
	);
	
	bool doSendMessageRelay(
		DynamicPointer<Message> &_rmsgptr,//the message to be sent
		const SerializationTypeIdT &_rtid,
		ConnectionUid *_pconid,
		const SocketAddressStub &_rsap,
		const uint32 _netid_dest,
		uint32	_flags
	);
	bool acceptSession(const SocketAddress &_rsa, const ConnectData &_rconndata);
	bool doAcceptBasicSession(const SocketAddress &_rsa, const ConnectData &_rconndata);
	bool doAcceptRelaySession(const SocketAddress &_rsa, const ConnectData &_rconndata);
	bool doAcceptGatewaySession(const SocketAddress &_rsa, const ConnectData &_rconndata);
	bool checkAcceptData(const SocketAddress &_rsa, const AcceptData &_raccdata);
	void disconnectSession(Session *_pses);
	void disconnectSession(const SocketAddressInet &_addr, uint32 _relayid);
	void disconnectTalkerSessions(Talker &, TalkerStub &_rts);
	void disconnectNodeSessions(Node &);
	int createTalker(IndexT &_tkrfullid, uint32 &_tkruid);
	int allocateTalkerForSession(bool _force = false);
	int createNode(IndexT &_nodepos, uint32 &_nodeuid);
	int allocateNodeForSession(bool _force = false);
	int allocateNodeForSocket(bool _force = false);
	uint32 keepAliveTimeout()const;
	void connectSession(const SocketAddressInet4 &_raddr);
	void insertConnection(
		SocketDevice &_rsd,
		aio::openssl::Context *_pctx,
		bool _secure
	);
	
	size_t	netId2AddressFind(uint32 _netid)const;
	Configuration::RelayAddress const&	netId2AddressAt(const size_t _off)const;
	size_t	netId2AddressVectorSize()const;
	
	size_t	address2NetIdFind(SocketAddressInet const&)const;
	Configuration::RelayAddress const&	address2NetIdAt(const size_t _off)const;
	size_t	address2NetIdVectorSize()const;
	
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
	ConnectionUid &_rconid,
	const SocketAddressStub &_rsa_dest,
	const uint32 _netid_dest,
	uint32	_flags
){
	return doSendMessage(_rmsgptr, SERIALIZATION_INVALIDID, &_rconid, _rsa_dest, _netid_dest, _flags);
}

inline bool Service::sendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SocketAddressStub &_rsa_dest,
	const uint32 _netid_dest,
	uint32	_flags
){
	return doSendMessage(_rmsgptr, SERIALIZATION_INVALIDID, NULL, _rsa_dest, _netid_dest, _flags);
}

inline bool Service::sendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SerializationTypeIdT &_rtid,
	ConnectionUid &_rconid,
	const SocketAddressStub &_rsa_dest,
	const uint32 _netid_dest,
	uint32	_flags
){
	return doSendMessage(_rmsgptr, _rtid, &_rconid, _rsa_dest, _netid_dest, _flags);
}

inline bool Service::sendMessage(
	DynamicPointer<Message> &_rmsgptr,//the message to be sent
	const SerializationTypeIdT &_rtid,
	const SocketAddressStub &_rsa_dest,
	const uint32 _netid_dest,
	uint32	_flags
){
	return doSendMessage(_rmsgptr, _rtid, NULL, _rsa_dest, _netid_dest, _flags);
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
