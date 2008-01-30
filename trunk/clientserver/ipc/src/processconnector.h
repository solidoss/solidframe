/* Declarations file processconnector.h
	
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

#ifndef IPC_PROCESS_CONNECTOR_H
#define IPC_PROCESS_CONNECTOR_H

#include "clientserver/core/cmdptr.h"
#include "iodata.h"
#include "system/timespec.h"

namespace serialization{
namespace bin{
class RTTIMapper;
}
}

struct SocketAddress;
struct Inet4SockAddrPair;
struct Inet6SockAddrPair;
struct TimeSpec;
struct AddrInfoIterator;

namespace clientserver{

struct Command;

namespace ipc{

struct Buffer;
struct IOData;
struct ServiceStubUnlocked;
class ProcessConnector;
struct ConnectorUid;
typedef serialization::bin::RTTIMapper BinMapper;

struct SendBufferData{
	SendBufferData():procid(0xffff),bufpos(0xffff),paddr(NULL){}
	uint16			procid;
	uint16			bufpos;
	SockAddrPair	*paddr;
	Buffer			b;
	TimeSpec		timeout;
};

//! The data identifying a process	
/*!
	Not all commands are guaranteed to still be sent on peer restart. These are the commands already written into discarded buffers.
	Should I discard sending commands (not responses) after its buffer was successfully sent!?
	Or should I consider the situation similar to, command successfully sent but peer crashed while processing it?!
	
	Considering the complexity of changes i think it is wize to chose the second situation.
*/
class ProcessConnector{
public:
	enum{
		LastPublicFlag = (1 << 16)
	};
	static void init(BinMapper &_rm);
	static int parseAcceptedBuffer(Buffer &_rbuf);
	static int parseConnectingBuffer(Buffer &_rbuf);
	
	ProcessConnector(BinMapper &_rm, const Inet4SockAddrPair &_raddr);
	//ProcessConnector(BinMapper &_rm, const Inet6SockAddrPair &_raddr);
	ProcessConnector(BinMapper &_rm, const Inet4SockAddrPair &_raddr, int _basport);
	//ProcessConnector(BinMapper &_rm, const Inet6SockAddrPair &_raddr, int _basport);
	~ProcessConnector();
	/**
	 * \retval BAD on error, 
	 *	NOK enqueued and talker must be waken, OK enqueued - no need to wake talker
	 */
	int pushCommand(clientserver::CmdPtr<Command> &_rcmd, uint32 _flags);
	int pushSentBuffer(SendBufferData &_rbuf, const TimeSpec &_tpos, bool &_reusebuf);
	int processSendCommands(SendBufferData &_rsb, const TimeSpec &_tpos, int _baseport);
	int pushReceivedBuffer(Buffer &_rbuf, const ConnectorUid &_rcodid);
	void completeConnect(int _port);
	void reconnect(ProcessConnector *_ppc);
	const Inet4SockAddrPair* peerAddr4()const;
	const std::pair<const Inet4SockAddrPair*, int>* baseAddr4()const;
	bool isConnected()const;
	bool isDisconnecting()const;
	bool isConnecting()const;
	//const Inet6SockAddrPair* pairAddr6()const;
	//const std::pair<const Inet6SockAddrPair*, int>* baseAddr6()const;
private:
	bool freeSentBuffers(Buffer &_rbuf);
	void parseBuffer(Buffer &_rbuf, const ConnectorUid &_rcodid);
	bool moveToNextInBuffer();
	//void popBuffersConnecting(IOData &_riod, const TimeSpec &_tpos, ServiceStubUnlocked &_rs);
	//void popBuffersConnected(IOData &_riod, const TimeSpec &_tpos, ServiceStubUnlocked &_rs);
	struct Data;
	Data	&d;
};

}//namespace ipc
}//namespace clientserver


#endif
