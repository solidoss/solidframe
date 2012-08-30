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

//#include "ipcdata.hpp"
#include "ipctalker.hpp"
#include "system/timespec.hpp"

#include "utility/dynamicpointer.hpp"

#include "foundation/ipc/ipcconnectionuid.hpp"
#include "ipcutility.hpp"

struct SocketAddress;
struct SocketAddressStub;
struct SocketAddressStub4;
struct SocketAddressStub6;
struct TimeSpec;
struct ResolveIterator;


namespace foundation{

struct Signal;

namespace ipc{


struct Context{
	static Context& the();
	Context(uint32 _tkrid);
	~Context();
	ConnectionContext sigctx;
};

class Session{
public:
	
	static void init();
	static int parseAcceptedBuffer(const Buffer &_rbuf);
	static int parseConnectingBuffer(const Buffer &_rbuf);
	
	Session(
		const SocketAddressInet4 &_raddr,
		uint32 _keepalivetout
	);
	Session(
		const SocketAddressInet4 &_raddr,
		uint16 _baseport,
		uint32 _keepalivetout
	);
	
	
	Session(
		const SocketAddressInet6 &_raddr,
		uint32 _keepalivetout
	);
	Session(
		const SocketAddressInet6 &_raddr,
		uint16 _baseport,
		uint32 _keepalivetout
	);
	
	~Session();
	
	const BaseAddress4T peerBaseAddress4()const;
	const BaseAddress6T peerBaseAddress6()const;
	
	//used by talker for sendto
	const SocketAddressStub& peerAddress()const;
	
	const SocketAddressInet4& peerAddress4()const;
	const SocketAddressInet6& peerAddress6()const;
	
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
	
	void completeConnect(Talker::TalkerStub &_rstub, uint16 _port);
	
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
	struct DataDirect4;
	struct DataDirect6;
	struct DataRelayed44;
	Data	&d;
};

}//namespace ipc
}//namespace foundation


#endif
