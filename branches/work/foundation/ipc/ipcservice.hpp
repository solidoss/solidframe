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

struct SocketAddressPair;
struct SocketDevice;
struct SocketAddressInfoIterator;
struct SocketAddressPair4;

namespace foundation{

namespace aio{
class Object;
}

namespace ipc{

class Session;
class Talker;
class Connection;
struct Buffer;
struct ConnectionUid;
struct BufferContext;

struct Controller{
	enum{
		HasAuthenticationFlag = 1,
	};
	virtual ~Controller();
	virtual bool release() = 0;
	virtual void scheduleTalker(foundation::aio::Object *_ptkr) = 0;
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
	bool hasAuthentication()const{
		return (flags & HasAuthenticationFlag) != 0;
	}
	
	virtual bool receive(
		Signal *_psig,
		ipc::SignalUid &_rsiguid
	);
	
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
protected:
	Controller(
		const uint32 _resdatasz = 0,
			const uint32 _flags = 0
	): resdatasz(_resdatasz), flags(_flags){}
	char * allocateBuffer(BufferContext &_rbc, uint32 &_cp);
private:
	uint32 		resdatasz;
	uint32		flags;
};


class Service;

//! A Inter Process Communication service
/*!
	<b>Overview:</b><br>
	- In its current state it only uses UDP for communication.
	- It can only send/receive signal objects (objects of a type
	derived from foundation::Signal).
	- It uses non portable, binary serialization, aiming for speed
	not for versatility.
	- For udp communication uses connectors which resembles somehow
	the tcp connections.
	- There can be at most one connector linking two processes, and it 
	is created when first send something.
	- More than one connectors are handled by a single 
	foundation::udp::Talker. There is a base connector used for both
	communication and accepting/creating new connectors. Also new talkers
	can be created when the number of connectors increases.
	- Because there can be only one connector to a peer process, and 
	signals can be quite big (i.e. signals sending streams) a signal
	multiplexing algorithm is implemented.
	- An existing connector can be identified by it unique id: 
	foundation::ipc::ConnectionUid or by its base address (inetaddr and port).
	- The ipc service should (i.e. it is a bug if it doesnt) ensure that
	 a response (or a signal sent using SameConnectorFlag) will not
	 be sent if the a peer process restart is detected.
	
	<b>Usage:</b><br>
	- On manager init, add the base taker to the ipc::Service
	- Then make your signals serializable
	- Send signals using a ipc::Service::sendSignal method.
	
	<b>Notes:</b><br>
	- Despite it's simple interface, the ipc service is quite complex
	software because it must ensure reliable communication over udp,
	emulating somehow the tcp.<br>
	- There is no implementation of keep alive, so a peer process restart
	will only be detected when some communication happens in either way.<br>
	- The ipc library is a nice example of how powerfull and flexible is the
	asynchrounous communication engine.<br>
	- When needed, new talkers will be created with ports values starting 
	incrementally from baseaddress port + 1.<br>
	- using SynchronousSendFlag flag a signal can be made synchronous, which
	means that no other synchronous signals are sent while sending a synchronous
	signal. A synchronous signal can still be multiplexed with non synchronous
	ones. Here is the problem: suppose you have a big signal spanned over multiple
	ipc buffers, another signal sent after the first one, can arrive on the 
	peer side before the first bigger signal. If someone needs to ensure
	that some signals are delivered one after another, all he needs to do
	is make them Synchronous (using Synchronous). The signal will still be
	multiplexed with non-synchronous signals.<br>
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
	typedef serialization::IdTypeMapper<
		serialization::binary::Serializer,
		serialization::binary::Deserializer,
		SerializationTypeIdT
	> IdTypeMapper;
	
	static Service& the();
	static Service& the(const IndexT &_ridx);
	
	Service(
		Controller *_pc,
		uint32 _keepalivetout = 0/*no keepalive*/,
		uint32 _sespertkr = 1024,
		uint32 _tkrmaxcnt = 2
	);
	//! Destructor
	~Service();
	
	IdTypeMapper& typeMapper();
	
// 	template <class T>
// 	uint32 registerSerializationType(uint32 _pos){
// 		return typemapper.insert<T>(_pos);
// 	}
// 	
// 	template <class T>
// 	uint32 registerSerializationType(){
// 		return typemapper.insert<T>();
// 	}
// 	
// 	template <class T, typename P>
// 	uint32 registerSerializationType(uint32 _pos){
// 		return typemapper.insert<T, P>(_pos);
// 	}
// 	
// 	template <class T, typename P>
// 	uint32 registerSerializationType(){
// 		return typemapper.insert<T, P>();
// 	}
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
		const SocketAddressPair &_rsap,
		ConnectionUid &_rconid,
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
		const SocketAddressPair &_rsap,
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
		const SocketAddressPair &_rsap,
		ConnectionUid &_rconid,
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
		const SocketAddressPair &_rsap,
		uint32	_flags = 0
	);
	
	//! Not used for now - will be used when ipc will use tcp connections
	int insertConnection(
		const SocketDevice &_rsd
	);
	//! Not used for now - will be used when ipc will use tcp connections
	int insertListener(
		const SocketAddressInfoIterator &_rai
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
		const SocketAddressInfoIterator &_rai
	);
	//! Not used for now - will be used when ipc will use tcp connections
	int insertConnection(
		const SocketAddressInfoIterator &_rai
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
	int doSendSignal(
		DynamicPointer<Signal> &_psig,//the signal to be sent
		const SerializationTypeIdT &_rtid,
		const SocketAddressPair &_rsap,
		ConnectionUid *_pconid,
		uint32	_flags = 0
	);
	int acceptSession(Session *_pses);
	void disconnectSession(Session *_pses);
	void disconnectTalkerSessions(Talker &);
	int createNewTalker(IndexT &_tkrpos, uint32 &_tkruid);
	int allocateTalkerForNewSession(bool _force = false);
	uint32 keepAliveTimeout()const;
	void connectSession(const SocketAddressPair4 &_raddr);
	Controller& controller();
private:
	struct Data;
	friend struct Data;
	Data			&d;
	IdTypeMapper	typemapper;
};

inline int Service::sendSignal(
	DynamicPointer<Signal> &_psig,//the signal to be sent
	const SocketAddressPair &_rsap,
	ConnectionUid &_rconid,
	uint32	_flags
){
	return doSendSignal(_psig, SERIALIZATION_INVALIDID, _rsap, &_rconid, _flags);
}

inline int Service::sendSignal(
	DynamicPointer<Signal> &_psig,//the signal to be sent
	const SocketAddressPair &_rsap,
	uint32	_flags
){
	return doSendSignal(_psig, SERIALIZATION_INVALIDID, _rsap, NULL, _flags);
}

inline int Service::sendSignal(
	DynamicPointer<Signal> &_psig,//the signal to be sent
	const SerializationTypeIdT &_rtid,
	const SocketAddressPair &_rsap,
	ConnectionUid &_rconid,
	uint32	_flags
){
	return doSendSignal(_psig, _rtid, _rsap, &_rconid, _flags);
}

inline int Service::sendSignal(
	DynamicPointer<Signal> &_psig,//the signal to be sent
	const SerializationTypeIdT &_rtid,
	const SocketAddressPair &_rsap,
	uint32	_flags
){
	return doSendSignal(_psig, _rtid, _rsap, NULL, _flags);
}

inline const serialization::TypeMapperBase& Service::typeMapperBase() const{
	return typemapper;
}

inline Service::IdTypeMapper& Service::typeMapper(){
	return typemapper;
}

}//namespace ipc
}//namespace foundation

#endif
