/* Declarations file ipcservice.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef IPCSERVICE_HPP
#define IPCSERVICE_HPP

#include "foundation/service.hpp"
#include "foundation/signal.hpp"
#include "foundation/ipc/connectoruid.hpp"

struct SockAddrPair;
struct SocketDevice;
struct AddrInfoIterator;

namespace foundation{

namespace aio{
class Object;
}

namespace ipc{

class Connection;
class Talker;
class ProcessConnector;
class IOData;
struct Buffer;
struct ConnectorUid;

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
	foundation::ipc::ConnectorUid or by its base address (inetaddr and port).
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
	emulating somehow the tcp.
	- There is no implementation of keep alive, so a peer process restart
	will only be detected when some communication happens in either way.
	- The ipc library is a nice example of how powerfull and flexible is the
	asynchrounous communication engine.
	- When needed, new talkers will be created with ports values starting 
	incrementally from baseaddress port + 1.
*/
class Service: public foundation::Service{
public:
	enum {
		SameConnectorFlag = 1, //!< Do not send signal to a restarted peer process
		ResponseFlag	= SameConnectorFlag, //!< The sent signal is a response
		WaitResponseFlag = 2,
		SentFlag = 4,//!< The signal was successfully sent
	};
	//! Destructor
	~Service();
	//!Send a signal (usually a response) to a peer process using a previously saved ConnectorUid
	/*!
		The signal is send only if the connector exists. If the peer process,
		restarts the signal is not sent.
		\param _rconid A previously saved connectionuid
		\param _psig A SignalPointer with the signal to be sent.
		\param _flags (Optional) Not used for now
	*/
	int sendSignal(
		const ConnectorUid &_rconid,//the id of the process connector
		foundation::SignalPointer<Signal> &_psig,//the signal to be sent
		uint32	_flags = 0
	);
	//!Send a signal to a peer process using it's base address.
	/*!
		The base address of a process is the address on which the process listens for new UDP connections.
		If the connection does not already exist, it will be created.
		\param _rsap The base socket address of the peer.
		\param _psig The signal.
		\param _rcondid An output value, which on success will contain the uid of the connector.
		\param _flags (Optional) Not used for now
	*/
	int sendSignal(
		const SockAddrPair &_rsap,
		foundation::SignalPointer<Signal> &_psig,//the signal to be sent
		ConnectorUid &_rconid,
		uint32	_flags = 0
	);
	//!Send a signal to a peer process using it's base address.
	/*!
		The base address of a process is the address on which the process listens for new UDP connections.
		If the connection does not already exist, it will be created.
		\param _rsap The base socket address of the peer.
		\param _psig The signal.
		\param _flags (Optional) Not used for now
	*/
	int sendSignal(
		const SockAddrPair &_rsap,
		foundation::SignalPointer<Signal> &_psig,//the signal to be sent
		uint32	_flags = 0
	);
	//! Not used for now - will be used when ipc will use tcp connections
	int insertConnection(
		Manager &_rm,
		const SocketDevice &_rsd
	);
	//! Not used for now - will be used when ipc will use tcp connections
	int insertListener(
		Manager &_rm,
		const AddrInfoIterator &_rai
	);
	//! Use this method to add the base talker
	/*!
		Should be called only once at manager initation.
		\param _rs Reference to manager
		\param _rai 
	*/
	int insertTalker(
		Manager &_rm, 
		const AddrInfoIterator &_rai,
		const char *_node,
		const char *_svc
	);
	//! Not used for now - will be used when ipc will use tcp connections
	int insertConnection(
		Manager &_rm,
		const AddrInfoIterator &_rai,
		const char *_node,
		const char *_svc
	);
	//! Not used for now - will be used when ipc will use tcp connections
	int removeConnection(Connection &);
	//! Called by a talker when it's destroyed
	int removeTalker(Talker&);
	//! Returns the value of the base port as set for the basetalker
	int basePort()const;
protected:
	int execute(ulong _sig, TimeSpec &_rtout);
	Service(uint32 _keepalivetout = 0/*no keepalive*/);
	virtual void pushTalkerInPool(foundation::Manager &_rs, foundation::aio::Object *_ptkr) = 0;
private:
	friend class Talker;
	int doSendSignal(
		const SockAddrPair &_rsap,
		foundation::SignalPointer<Signal> &_psig,//the signal to be sent
		ConnectorUid *_pconid,
		uint32	_flags = 0
	);
	int acceptProcess(ProcessConnector *_ppc);
	void disconnectProcess(ProcessConnector *_ppc);
	void disconnectTalkerProcesses(Talker &);
	int16 createNewTalker(uint32 &_tkrpos, uint32 &_tkruid);
	int16 computeTalkerForNewProcess();
	uint32 keepAliveTimeout()const;
private:
	struct Data;
	friend struct Data;
	Data	&d;
};

inline int Service::sendSignal(
	const SockAddrPair &_rsap,
	foundation::SignalPointer<Signal> &_psig,//the signal to be sent
	ConnectorUid &_rconid,
	uint32	_flags
){
	return doSendSignal(_rsap, _psig, &_rconid, _flags);
}

inline int Service::sendSignal(
	const SockAddrPair &_rsap,
	foundation::SignalPointer<Signal> &_psig,//the signal to be sent
	uint32	_flags
){
	return doSendSignal(_rsap, _psig, NULL, _flags);
}

}//namespace ipc
}//namespace foundation

#endif
