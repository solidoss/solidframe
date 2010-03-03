/* Declarations file processconnector.hpp
	
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

#ifndef IPC_PROCESS_CONNECTOR_HPP
#define IPC_PROCESS_CONNECTOR_HPP


#include "iodata.hpp"
#include "system/timespec.hpp"

#include "utility/dynamicpointer.hpp"

struct SocketAddress;
struct Inet4SockAddrPair;
struct Inet6SockAddrPair;
struct TimeSpec;
struct AddrInfoIterator;

namespace foundation{

struct Signal;

namespace ipc{

struct Buffer;
struct IOData;
struct ServiceStubUnlocked;
class ProcessConnector;
struct ConnectorUid;

struct SendBufferData{
	SendBufferData():procid(0xffff),bufpos(0xffff),paddr(NULL){}
	uint16			procid;
	uint16			bufpos;
	SockAddrPair	*paddr;
	Buffer			b;
	TimeSpec		timeout;
};

//! Handles a virtual connection to a peer process
/*!
	It implements capabilities like: in order buffer receive, keep alive support,
	multiplexed signal sending/receiving.
	This class is not used directly.
*/
class ProcessConnector{
public:
	enum{
		LastPublicFlag = (1 << 16)
	};
	static void init();
	static int parseAcceptedBuffer(Buffer &_rbuf);
	static int parseConnectingBuffer(Buffer &_rbuf);
	
	ProcessConnector(const Inet4SockAddrPair &_raddr, uint32 _keepalivetout);
	ProcessConnector(const Inet4SockAddrPair &_raddr, int _basport, uint32 _keepalivetout);
	~ProcessConnector();
	int pushSignal(DynamicPointer<Signal> &_rsig, uint32 _flags);
	int pushSentBuffer(SendBufferData &_rbuf, const TimeSpec &_tpos, bool &_reusebuf);
	int processSendSignals(SendBufferData &_rsb, const TimeSpec &_tpos, int _baseport);
	int pushReceivedBuffer(Buffer &_rbuf, const ConnectorUid &_rcodid, const TimeSpec &_tpos);
	void completeConnect(int _port);
	void reconnect(ProcessConnector *_ppc);
	const Inet4SockAddrPair* peerAddr4()const;
	const std::pair<const Inet4SockAddrPair*, int>* baseAddr4()const;
	bool isConnected()const;
	bool isDisconnecting()const;
	bool isConnecting()const;
	bool isAccepting()const;
	void prepare();
	//const Inet6SockAddrPair* pairAddr6()const;
	//const std::pair<const Inet6SockAddrPair*, int>* baseAddr6()const;
private:
	bool freeSentBuffers(Buffer &_rbuf, const ConnectorUid &_rconid);
	void parseBuffer(Buffer &_rbuf, const ConnectorUid &_rconid);
	bool moveToNextInBuffer();
	void freeBufferSignals(uint32 _bufid, const ConnectorUid &_rconid);//frees the signals associated to a send buffer
	//void popBuffersConnecting(IOData &_riod, const TimeSpec &_tpos, ServiceStubUnlocked &_rs);
	//void popBuffersConnected(IOData &_riod, const TimeSpec &_tpos, ServiceStubUnlocked &_rs);
	struct Data;
	Data	&d;
};

}//namespace ipc
}//namespace foundation


#endif
