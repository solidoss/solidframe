/* Declarations file ipcservice.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef IPCSERVICE_H
#define IPCSERVICE_H

#include "clientserver/core/service.h"
#include "clientserver/core/command.h"
#include "clientserver/ipc/connectoruid.h"

namespace serialization{
namespace bin{
class RTTIMapper;
}
}

struct SockAddrPair;

namespace clientserver{

namespace tcp{
class Channel;
class Station;
}

namespace udp{
class Station;
class Talker;
}


namespace ipc{

class Connection;
class Talker;
class ProcessConnector;
class IOData;
struct Buffer;
struct ConnectorUid;

class Service;

//struct ConnectorUid;
/*!
NOTE:
	No response is sent on peer restart. Commands are send.
*/
class Service: public clientserver::Service{
public:
	enum {
		SameConnectorFlag = 1,
		ResponseFlag	= SameConnectorFlag
	};
	//static Service* create(serialization::bin::RTTIMapper&);
	~Service();
	//!Send a command (usually a response) to a peer process using a previously saved ConnectorUid
	/*!
		The command is send only if the connector exists. If the peer process,
		restarts the command is not sent.
		\param _rconid A previously saved connectionuid
		\param _pcmd A CmdPtr with the command to be sent.
		\param _flags (Optional) Not used for now
	*/
	int sendCommand(
		const ConnectorUid &_rconid,//the id of the process connector
		clientserver::CmdPtr<Command> &_pcmd,//the command to be sent
		uint32	_flags = 0
	);
	//!Send a command to a peer process using it's base address.
	/*!
		The base address of a process is the address on which the process listens for new UDP connections.
		Yes internally the ipc service uses reliable connections over udp.
		If the connection does not already exist, it will be created.
		\param _rsap The base socket address of the peer.
		\param _pcmd The command.
		\param _rcondid An output value, which on success will contain the uid of the connector.
		\param _flags (Optional) Not used for now
	*/
	int sendCommand(
		const SockAddrPair &_rsap,
		clientserver::CmdPtr<Command> &_pcmd,//the command to be sent
		ConnectorUid &_rconid,
		uint32	_flags = 0
	);
	int sendCommand(
		const SockAddrPair &_rsap,
		clientserver::CmdPtr<Command> &_pcmd,//the command to be sent
		uint32	_flags = 0
	);
	/**
	 */
	int insertConnection(
		Server &_rs,
		clientserver::tcp::Channel *_pch
	);
	int insertListener(
		Server &_rs,
		const AddrInfoIterator &_rai
	);
	int insertTalker(
		Server &_rs, 
		const AddrInfoIterator &_rai,
		const char *_node,
		const char *_svc
	);
	int insertConnection(
		Server &_rs,
		const AddrInfoIterator &_rai,
		const char *_node,
		const char *_svc
	);
	int removeConnection(Connection &);
	int removeTalker(Talker&);
	int basePort()const;
protected:
	int execute(ulong _sig, TimeSpec &_rtout);
	Service(serialization::bin::RTTIMapper&);
	virtual void pushTalkerInPool(clientserver::Server &_rs, clientserver::udp::Talker *_ptkr) = 0;
private:
	friend class Talker;
	int doSendCommand(
		const SockAddrPair &_rsap,
		clientserver::CmdPtr<Command> &_pcmd,//the command to be sent
		ConnectorUid *_pconid,
		uint32	_flags = 0
	);
	int acceptProcess(ProcessConnector *_ppc);
	void disconnectProcess(ProcessConnector *_ppc);
	void disconnectTalkerProcesses(Talker &);
	int16 createNewTalker(uint32 &_tkrpos, uint32 &_tkruid);
	int16 computeTalkerForNewProcess();
private:
	struct Data;
	friend struct Data;
	Data	&d;
};

inline int Service::sendCommand(
	const SockAddrPair &_rsap,
	clientserver::CmdPtr<Command> &_pcmd,//the command to be sent
	ConnectorUid &_rconid,
	uint32	_flags
){
	return doSendCommand(_rsap, _pcmd, &_rconid, _flags);
}

inline int Service::sendCommand(
	const SockAddrPair &_rsap,
	clientserver::CmdPtr<Command> &_pcmd,//the command to be sent
	uint32	_flags
){
	return doSendCommand(_rsap, _pcmd, NULL, _flags);
}

}//namespace ipc
}//namespace clientserver

#endif
