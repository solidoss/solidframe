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

#ifndef FOUNDATION_IPC_SRC_IPC_SESSION_HPP
#define FOUNDATION_IPC_SRC_IPC_SESSION_HPP

#include "iodata.hpp"
#include "ipctalker.hpp"
#include "system/timespec.hpp"

#include "utility/dynamicpointer.hpp"

#include "foundation/ipc/ipcconnectionuid.hpp"

struct SocketAddress;
struct SocketAddressPair;
struct SocketAddressPair4;
struct SocketAddressPair6;
struct TimeSpec;
struct SocketAddressInfoIterator;


namespace foundation{

struct Signal;

namespace ipc{


struct Context{
	static Context& the();
	Context(uint32 _tkrid);
	~Context();
	SignalContext sigctx;
};

class Session{
public:
	typedef std::pair<const SocketAddressPair4*, int> Addr4PairT;
	typedef std::pair<const SocketAddressPair6*, int> Addr6PairT;
public:
	static void init();
	static int parseAcceptedBuffer(const Buffer &_rbuf);
	static int parseConnectingBuffer(const Buffer &_rbuf);
	
	Session(
		const SocketAddressPair4 &_raddr,
		uint32 _keepalivetout
	);
	Session(
		const SocketAddressPair4 &_raddr,
		int _basport,
		uint32 _keepalivetout
	);
	
	~Session();
	
	const SocketAddressPair4* peerAddr4()const;
	const Addr4PairT* baseAddr4()const;
	const SocketAddressPair* peerSockAddr()const;
	
	bool isConnected()const;
	bool isDisconnecting()const;
	bool isDisconnected()const;
	bool isConnecting()const;
	bool isAccepting()const;

	void prepare();
	void reconnect(Session *_pses);	
	int pushSignal(
		DynamicPointer<Signal> &_rsig,
		const SerializationTypeIdT &_rtid,
		uint32 _flags
	);
	bool pushReceivedBuffer(
		Buffer &_rbuf,
		Talker::TalkerStub &_rstub/*,
		const ConnectionUid &_rconid*/
	);
	
	void completeConnect(Talker::TalkerStub &_rstub, int _port);
	
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
	void prepareContext(Context &_rctx);
private:
	bool doPushExpectedReceivedBuffer(
		Talker::TalkerStub &_rstub,
		Buffer &_rbuf
		/*,
		const ConnectionUid &_rconid*/
	);
	bool doPushUnxpectedReceivedBuffer(
		Talker::TalkerStub &_rstub,
		Buffer &_rbuf
		/*,
		const ConnectionUid &_rconid*/
	);
	bool doFreeSentBuffers(const Buffer &_rbuf/*, const ConnectionUid &_rconid*/);
	void doParseBufferDataType(Talker::TalkerStub &_rstub, const char *&_bpos, int &_blen, int _firstblen);
	void doParseBuffer(Talker::TalkerStub &_rstub, const Buffer &_rbuf/*, const ConnectionUid &_rconid*/);
	
	int doExecuteConnecting(Talker::TalkerStub &_rstub);
	int doExecuteAccepting(Talker::TalkerStub &_rstub);
	int doExecuteConnected(Talker::TalkerStub &_rstub);
	int doExecuteConnectedLimited(Talker::TalkerStub &_rstub);
	int doTrySendUpdates(Talker::TalkerStub &_rstub);
	int doExecuteDisconnecting(Talker::TalkerStub &_rstub);
	void doFillSendBuffer(Talker::TalkerStub &_rstub, const uint32 _bufidx);
	void doTryScheduleKeepAlive(Talker::TalkerStub &_rstub);
private:
	struct Data;
	Data	&d;
};

}//namespace ipc
}//namespace foundation


#endif
