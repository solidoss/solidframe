/* Declarations file ipcsession.hpp
	
	Copyright 2010 Valentin Palade
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

#ifndef FOUNDATION_IPC_SRC_IPC_CONNECTION_HPP
#define FOUNDATION_IPC_SRC_IPC_CONNECTION_HPP

#include "iodata.hpp"
#include "ipctalker.hpp"
#include "system/timespec.hpp"

#include "utility/dynamicpointer.hpp"

struct SocketAddress;
struct SockAddrPair;
struct Inet4SockAddrPair;
struct Inet6SockAddrPair;
struct TimeSpec;
struct AddrInfoIterator;


namespace foundation{

struct Signal;

namespace ipc{

class Session{
public:
	typedef std::pair<const Inet4SockAddrPair*, int> Addr4PairT;
	typedef std::pair<const Inet6SockAddrPair*, int> Addr6PairT;
public:
	static void init();
	static int parseAcceptedBuffer(const Buffer &_rbuf);
	static int parseConnectingBuffer(const Buffer &_rbuf);
	
	Session(
		const Inet4SockAddrPair &_raddr,
		uint32 _keepalivetout
	);
	Session(
		const Inet4SockAddrPair &_raddr,
		int _basport,
		uint32 _keepalivetout
	);
	
	~Session();
	
	const Inet4SockAddrPair* peerAddr4()const;
	const Addr4PairT* baseAddr4()const;
	const SockAddrPair* peerSockAddr()const;
	
	bool isConnected()const;
	bool isDisconnecting()const;
	bool isConnecting()const;
	bool isAccepting()const;

	void prepare();
	void reconnect(Session *_pses);	
	int pushSignal(DynamicPointer<Signal> &_rsig, uint32 _flags);
	bool pushReceivedBuffer(
		Buffer &_rbuf,
		Talker::TalkerStub &_rstub,
		const ConnectionUid &_rconid
	);
	
	void completeConnect(int _port);
	
	bool executeTimeout(
		Talker::TalkerStub &_rstub,
		uint32 _id
	);
	int execute(Talker::TalkerStub &_rstub);
	bool pushSentBuffer(
		Talker::TalkerStub &_rstub,
		uint32 _id,
		const char *_data,
		const uint16 _size
	);
private:
	bool doPushExpectedReceivedBuffer(
		Buffer &_rbuf,
		Talker::TalkerStub &_rstub,
		const ConnectionUid &_rconid
	);
	bool doPushUnxpectedReceivedBuffer(
		Buffer &_rbuf,
		Talker::TalkerStub &_rstub,
		const ConnectionUid &_rconid
	);
	bool doFreeSentBuffers(const Buffer &_rbuf, const ConnectionUid &_rconid);
	void doParseBufferDataType(const char *&_bpos, int &_blen, int _firstblen);
	void doParseBuffer(const Buffer &_rbuf, const ConnectionUid &_rconid);
	
	int doExecuteConnecting(Talker::TalkerStub &_rstub);
	int doExecuteAccepting(Talker::TalkerStub &_rstub);
	int doExecuteConnected(Talker::TalkerStub &_rstub);
	int doExecuteDisconnect(Talker::TalkerStub &_rstub);
	void doFillSendBuffer(const uint32 _bufidx);
	void doTryScheduleKeepAlive(Talker::TalkerStub &_rstub);
private:
	struct Data;
	Data	&d;
};

}//namespace ipc
}//namespace foundation


#endif
