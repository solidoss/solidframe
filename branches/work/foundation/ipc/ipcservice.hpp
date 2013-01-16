/* Declarations file ipcservice.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FOUNDATION_IPC_IPCSERVICE_HPP
#define FOUNDATION_IPC_IPCSERVICE_HPP

#include "algorithm/serialization/idtypemapper.hpp"
#include "algorithm/serialization/binary.hpp"

#include "foundation/service.hpp"
#include "foundation/signal.hpp"
#include "foundation/ipc/ipcconnectionuid.hpp"

struct SocketAddressStub;
struct SocketDevice;
struct ResolveIterator;

namespace foundation{

namespace aio{
class Object;

namespace openssl{
class Context;
}

}

namespace ipc{

class Session;
class Talker;
class Connection;
struct Buffer;
struct ConnectionUid;
struct BufferContext;
struct ConnectData;
struct AcceptData;
class Service;

struct Controller: Dynamic<Controller, DynamicShared<> >{
	enum{
		AuthenticationFlag = 1,
		GatewayFlag = 2
	};
	
	virtual ~Controller();
	
	virtual void scheduleTalker(foundation::aio::Object *_ptkr) = 0;
	virtual void scheduleListener(foundation::aio::Object *_ptkr) = 0;
	virtual void scheduleNode(foundation::aio::Object *_ptkr) = 0;
	
	virtual bool compressBuffer(
		BufferContext &_rbc,
		const uint32 _bufsz,
		char* &_rpb,
		uint32 &_bl
	);
	virtual bool decompressBuffer(
		BufferContext &_rbc,
		char* &_rpb,
		uint32 &_bl
	);
	
	virtual SocketAddressStub gatewayAddress(
		const uint _idx,
		const uint32 _netid_dest,
		const SocketAddressStub &_rsas_dest
	);
	
	//retval:
	// -1 : wait for asynchrounous event and retry
	// 0: no gateway
	// > 0: the count
	virtual int gatewayCount(
		const uint32 _netid_dest,
		const SocketAddressStub &_rsas_dest
	)const;
	
	//called on the gateway to find out where to connect for relaying data to _rsas_dest
	virtual const SocketAddress& relayAddress(
		const uint32 _netid_dest,
		const SocketAddressStub &_rsas_dest
	);
	
	//called on the gateway to find out where to connect for relaying data to _rsas_dest
	virtual uint32 relayCount(
		const uint32 _netid_dest,
		const SocketAddressStub &_rsas_dest
	)const;
	
	bool isLocalNetwork(
		const SocketAddressStub &_rsas_dest,
		const uint32 _netid_dest
	)const;
	
	bool hasAuthentication()const{
		return (flags & AuthenticationFlag) != 0;
	}
	
	bool isGateway()const{
		return (flags & GatewayFlag) != 0;
	}
	
	virtual bool receive(
		Signal *_psig,
		ipc::SignalUid &_rsiguid
	);
	
	virtual uint32 localNetworkId()const;
	
	//Must return:
	// If _psig is NULL, the request is to initiate the authentication,
	// and _psig must be filled with a command to be sent to peer.
	// * OK for authentication competed with success - session enters in AUTHENTICATED state
	// * NOK authentication not completed - session stays in AUTHENTICATED state
	// * BAD authentication completed with error - session enters Disconnecting
	virtual int authenticate(
		DynamicPointer<Signal> &_sigptr,//the received signal
		ipc::SignalUid &_rsiguid,
		uint32 &_rflags,
		SerializationTypeIdT &_rtid
	);
	const uint32 reservedDataSize()const{
		return resdatasz;
	}
	
	const uint32 responseKeepAlive()const;
	const uint32 sessionKeepAlive()const;
	
	DynamicPointer<Controller> pointer(){
		return DynamicPointer<Controller>(this);
	}
	
protected:
	Controller(
		const uint32 _resdatasz = 0,
		const uint32 _flags = 0,
		const uint32 _reskeepalive = 0,
		const uint32 _seskeepalive = 60 * 1000
	):	resdatasz(_resdatasz), flags(_flags),
		reskeepalive(
			_reskeepalive < _seskeepalive 
			? 
			(_reskeepalive == 0 ? _seskeepalive : _reskeepalive )
			: 
			(_seskeepalive == 0 ? _reskeepalive : _seskeepalive)
		),
		seskeepalive(_seskeepalive){}
	
	char * allocateBuffer(BufferContext &_rbc, uint32 &_cp);
	
	void sendEvent(
		Service &_rs,
		const ConnectionUid &_rconid,//the id of the process connectors
		int32 _event,
		uint32 _flags
	);
private:
	const uint32	resdatasz;
	uint32			flags;
	const uint32	reskeepalive;
	const uint32	seskeepalive;
};

//! An Inter Process Communication service
/*!
	Allow for sending/receiving serializable foundation::Signal objects between
	processes.
	For processes within the same network, the signals are sent using a reliable
	protocol based on UDP.
	For processes within different networks, SolidFrame allows for configuring
	gateway processes which will allow tunneling the ipc signals between 
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
class Service: public Dynamic<Service, foundation::Service>{
public:
	enum {
		SameConnectorFlag = 1, //!< Do not send signal to a restarted peer process
		ResponseFlag	= SameConnectorFlag, //!< The sent signal is a response
		WaitResponseFlag = 2,
		SynchronousSendFlag = 4,//!< Make the signal synchronous
		SentFlag = 8,//!< The signal was successfully sent
		AuthenticationFlag = 16,//!< The signal is for authentication
		DisconnectAfterSendFlag = 32,//!< Disconnect the session after sending the signal
	};
	
	enum Errors{
		NoError = 0,
		NoGatewayError = 100,
	};
	
	enum Types{
		PlainType = 1,
		RelayType
	};
	typedef serialization::IdTypeMapper<
		serialization::binary::Serializer,
		serialization::binary::Deserializer,
		SerializationTypeIdT
	> IdTypeMapperT;
	
	
	static Service& the();
	static Service& the(const IndexT &_ridx);
	
	static const char* errorText(int _err);
	
	Service(
		const DynamicPointer<Controller> &_rctrlptr,
		const uint32 _tkrsescnt = 1024,
		const uint32 _tkrmaxcnt = 2,
		const uint32 _nodesescnt = 1024,
		const uint32 _nodesockcnt = 256,
		const uint32 _nodemaxcnt = 2
	);
	//! Destructor
	~Service();
	
	IdTypeMapperT& typeMapper();
	const TimeSpec& timeStamp()const;
	
	//!Send a signal (usually a response) to a peer process using a previously saved ConnectionUid
	/*!
		The signal is send only if the connector exists. If the peer process,
		restarts the signal is not sent.
		\param _rconid A previously saved connectionuid
		\param _psig A DynamicPointer with the signal to be sent.
		\param _flags Control flags
	*/
	
	int sendSignal(
		DynamicPointer<Signal> &_psig,//the signal to be sent
		const ConnectionUid &_rconid,//the id of the process connector
		uint32	_flags = 0
	);
	
	//!Send a signal to a peer process using it's base address.
	/*!
		The base address of a process is the address on which the process listens for new UDP connections.
		If the connection does not already exist, it will be created.
		\param _rsap The base socket address of the peer.
		\param _psig The signal.
		\param _rconid An output value, which on success will contain the uid of the connector.
		\param _flags Control flags
	*/
	int sendSignal(
		DynamicPointer<Signal> &_psig,//the signal to be sent
		ConnectionUid &_rconid,
		const SocketAddressStub &_rsa_dest,
		const uint32 _netid_dest = LocalNetworkId,
		uint32	_flags = 0
	);
	//!Send a signal to a peer process using it's base address.
	/*!
		The base address of a process is the address on which the process listens for new UDP connections.
		If the connection does not already exist, it will be created.
		\param _rsap The base socket address of the peer.
		\param _psig The signal.
		\param _flags Control flags
	*/
	int sendSignal(
		DynamicPointer<Signal> &_psig,//the signal to be sent
		const SocketAddressStub &_rsas_dest,
		const uint32 _netid_dest = LocalNetworkId,
		uint32	_flags = 0
	);
	
	///////
	int sendSignal(
		DynamicPointer<Signal> &_psig,//the signal to be sent
		const SerializationTypeIdT &_rtid,
		const ConnectionUid &_rconid,//the id of the process connector
		uint32	_flags = 0
	);
	
	//!Send a signal to a peer process using it's base address.
	/*!
		The base address of a process is the address on which the process listens for new UDP connections.
		If the connection does not already exist, it will be created.
		\param _rsap The base socket address of the peer.
		\param _psig The signal.
		\param _rconid An output value, which on success will contain the uid of the connector.
		\param _flags Control flags
	*/
	int sendSignal(
		DynamicPointer<Signal> &_psig,//the signal to be sent
		const SerializationTypeIdT &_rtid,
		ConnectionUid &_rconid,
		const SocketAddressStub &_rsa_dest,
		const uint32 _netid_dest = LocalNetworkId,
		uint32	_flags = 0
	);
	//!Send a signal to a peer process using it's base address.
	/*!
		The base address of a process is the address on which the process listens for new UDP connections.
		If the connection does not already exist, it will be created.
		\param _rsap The base socket address of the peer.
		\param _psig The signal.
		\param _flags Control flags
	*/
	int sendSignal(
		DynamicPointer<Signal> &_psig,//the signal to be sent
		const SerializationTypeIdT &_rtid,
		const SocketAddressStub &_rsa_dest,
		const uint32 _netid_dest = LocalNetworkId,
		uint32	_flags = 0
	);
	
	//! Not used for now - will be used when ipc will use tcp connections
	int insertConnection(
		const SocketDevice &_rsd,
		const Types _type = PlainType,
		foundation::aio::openssl::Context *_pctx = NULL,
		bool _secure = false
	);
	//! Not used for now - will be used when ipc will use tcp connections
	int insertListener(
		const ResolveIterator &_rai,
		const Types _type = PlainType,
		bool _secure = false
	);
	//! Use this method to add the base talker
	/*!
		Should be called only once at manager initation.
		\param _rm Reference to manager
		\param _rai The address for talker to use
		\param _node  Address to connect to
		\param _svc Service/Port to connect to
	*/
	int insertTalker(
		const ResolveIterator &_rai
	);
	//! Not used for now - will be used when ipc will use tcp connections
	int insertConnection(
		const ResolveIterator &_rai
	);
	//! Not used for now - will be used when ipc will use tcp connections
	int removeConnection(Connection &);
	//! Called by a talker when it's destroyed
	int removeTalker(Talker&);
	//! Returns the value of the base port as set for the basetalker
	int basePort()const;
	
	void insertObject(Talker &_ro, const ObjectUidT &_ruid);
	
	void eraseObject(const Talker &_ro);
	
	const serialization::TypeMapperBase& typeMapperBase() const;
	
	
private:
	friend class Talker;
	friend class Session;
	friend class Controller;
	
	void doSendEvent(
		const ConnectionUid &_rconid,//the id of the process connectors
		int32 _event,
		uint32 _flags = 0
	);
	
	int doSendSignal(
		DynamicPointer<Signal> &_psig,//the signal to be sent
		const SerializationTypeIdT &_rtid,
		ConnectionUid *_pconid,
		const SocketAddressStub &_rsap,
		const uint32 _netid_dest,
		uint32	_flags = 0
	);
	int doSendSignalLocal(
		DynamicPointer<Signal> &_psig,//the signal to be sent
		const SerializationTypeIdT &_rtid,
		ConnectionUid *_pconid,
		const SocketAddressStub &_rsap,
		const uint32 _netid_dest,
		uint32	_flags
	);
	int doSendSignalRelay(
		DynamicPointer<Signal> &_psig,//the signal to be sent
		const SerializationTypeIdT &_rtid,
		ConnectionUid *_pconid,
		const SocketAddressStub &_rsap,
		const uint32 _netid_dest,
		uint32	_flags
	);
	int acceptSession(const SocketAddress &_rsa, const ConnectData &_rconndata);
	int doAcceptBasicSession(const SocketAddress &_rsa, const ConnectData &_rconndata);
	int doAcceptRelaySession(const SocketAddress &_rsa, const ConnectData &_rconndata);
	int doAcceptGatewaySession(const SocketAddress &_rsa, const ConnectData &_rconndata);
	bool checkAcceptData(const SocketAddress &_rsa, const AcceptData &_raccdata);
	void disconnectSession(Session *_pses);
	void disconnectTalkerSessions(Talker &);
	int createTalker(IndexT &_tkrpos, uint32 &_tkruid);
	int allocateTalkerForSession(bool _force = false);
	int createNode(IndexT &_nodepos, uint32 &_nodeuid);
	int allocateNodeForSession(bool _force = false);
	int allocateNodeForSocket(bool _force = false);
	uint32 keepAliveTimeout()const;
	void connectSession(const SocketAddressInet4 &_raddr);
	
	Controller& controller();
	
	const Controller& controller()const;
private:
	struct Data;
	friend struct Data;
	Data			&d;
	IdTypeMapperT	typemapper;
};

inline int Service::sendSignal(
	DynamicPointer<Signal> &_psig,//the signal to be sent
	ConnectionUid &_rconid,
	const SocketAddressStub &_rsa_dest,
	const uint32 _netid_dest,
	uint32	_flags
){
	return doSendSignal(_psig, SERIALIZATION_INVALIDID, &_rconid, _rsa_dest, _netid_dest, _flags);
}

inline const uint32 Controller::responseKeepAlive()const{
	return reskeepalive;
}
inline const uint32 Controller::sessionKeepAlive()const{
	return seskeepalive;
}


inline int Service::sendSignal(
	DynamicPointer<Signal> &_psig,//the signal to be sent
	const SocketAddressStub &_rsa_dest,
	const uint32 _netid_dest,
	uint32	_flags
){
	return doSendSignal(_psig, SERIALIZATION_INVALIDID, NULL, _rsa_dest, _netid_dest, _flags);
}

inline int Service::sendSignal(
	DynamicPointer<Signal> &_psig,//the signal to be sent
	const SerializationTypeIdT &_rtid,
	ConnectionUid &_rconid,
	const SocketAddressStub &_rsa_dest,
	const uint32 _netid_dest,
	uint32	_flags
){
	return doSendSignal(_psig, _rtid, &_rconid, _rsa_dest, _netid_dest, _flags);
}

inline int Service::sendSignal(
	DynamicPointer<Signal> &_psig,//the signal to be sent
	const SerializationTypeIdT &_rtid,
	const SocketAddressStub &_rsa_dest,
	const uint32 _netid_dest,
	uint32	_flags
){
	return doSendSignal(_psig, _rtid, NULL, _rsa_dest, _netid_dest, _flags);
}

inline const serialization::TypeMapperBase& Service::typeMapperBase() const{
	return typemapper;
}

inline Service::IdTypeMapperT& Service::typeMapper(){
	return typemapper;
}



}//namespace ipc
}//namespace foundation

#endif
