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

#ifndef SOLID_FRAME_IPC_SRC_IPC_SESSION_HPP
#define SOLID_FRAME_IPC_SRC_IPC_SESSION_HPP

//#include "ipcdata.hpp"
#include "ipctalker.hpp"
#include "system/timespec.hpp"

#include "utility/dynamicpointer.hpp"

#include "frame/ipc/ipcconnectionuid.hpp"
#include "ipcutility.hpp"

struct SocketAddress;
struct SocketAddressStub;
struct SocketAddressStub4;
struct SocketAddressStub6;
struct TimeSpec;
struct ResolveIterator;

namespace solid{
namespace frame{

struct Message;

namespace ipc{


struct Context{
	static Context& the();
	Context(Service &_srv, const IndexT &_tkrid, const uint32 _uid);
	~Context();
	ConnectionContext msgctx;
};

class Session{
public:
	
	static void init();
	static int parseAcceptBuffer(
		const Buffer &_rbuf,
		AcceptData &_raccdata,
		const SocketAddress &_rfromsa
	);
	static int parseConnectBuffer(
		const Buffer &_rbuf,
		ConnectData &_rconndata,
		const SocketAddress &_rfromsa
	);
	static int fillAcceptBuffer(
		Buffer &_rbuf,
		const AcceptData &_raccdata
	);
	static int fillConnectBuffer(
		Buffer &_rbuf,
		const ConnectData &_rconndata
	);
	
	static uint32 computeResendTime(const size_t _cnt);
	
	Session(
		Service &_rsvc,
		const SocketAddressInet4 &_raddr
	);
	Session(
		Service &_rsvc,
		const SocketAddressInet4 &_raddr,
		const ConnectData &_rconndata
	);
	
	Session(
		Service &_rsvc,
		const uint32 _netid,
		const SocketAddressInet4 &_raddr,
		const uint32 _gwidx
	);
	Session(
		Service &_rsvc,
		const uint32 _netid,
		const SocketAddressInet4 &_raddr,
		const ConnectData &_rconndata,
		const uint32 _gwidx
	);
	
	Session(
		Service &_rsvc,
		const SocketAddressInet6 &_raddr
	);
	Session(
		Service &_rsvc,
		const SocketAddressInet6 &_raddr,
		const ConnectData &_rconndata
	);
	
	Session(Service &_rsvc);
	
	~Session();
	
	bool isRelayType()const;
	
	const BaseAddress4T peerBaseAddress4()const;
	const BaseAddress6T peerBaseAddress6()const;
	
	const RelayAddress4T peerRelayAddress4()const;
	//const RelayAddress6T peerRelayAddress6()const;
	
	//used by talker for sendto
	const SocketAddressStub& peerAddress()const;
	
	const SocketAddressInet4& peerAddress4()const;
	const SocketAddressInet6& peerAddress6()const;
	
	bool isConnected()const;
	bool isDisconnecting()const;
	bool isDisconnected()const;
	bool isConnecting()const;
	bool isAccepting()const;

	void prepare(TalkerStub &_rstub);
	void reconnect(Session *_pses);	
	
	bool pushMessage(
		DynamicPointer<Message> &_rmsgptr,
		const SerializationTypeIdT &_rtid,
		uint32 _flags
	);
	
	bool pushEvent(
		int32 _event,
		uint32 _flags
	);
	
	bool preprocessReceivedBuffer(
		Buffer &_rbuf,
		TalkerStub &_rstub
	);
	
	bool pushReceivedBuffer(
		Buffer &_rbuf,
		TalkerStub &_rstub
	);
	
	bool pushReceivedErrorBuffer(
		Buffer &_rbuf,
		TalkerStub &_rstub
	);
	
	void completeConnect(TalkerStub &_rstub, uint16 _port);
	void completeConnect(TalkerStub &_rstub, uint16 _port, uint32 _relayid);
	
	bool executeTimeout(
		TalkerStub &_rstub,
		uint32 _id
	);
	
	int execute(TalkerStub &_rstub);
	
	bool pushSentBuffer(
		TalkerStub &_rstub,
		uint32 _id,
		const char *_data,
		const uint16 _size
	);
	
	void prepareContext(Context &_rctx);
	
	void dummySendError(TalkerStub &_rstub, const SocketAddress &_rsa, int _error);
private:
	bool doPushExpectedReceivedBuffer(
		TalkerStub &_rstub,
		Buffer &_rbuf
	);
	bool doPushUnxpectedReceivedBuffer(
		TalkerStub &_rstub,
		Buffer &_rbuf
	);
	bool doFreeSentBuffers(const Buffer &_rbuf);
	void doParseBufferDataType(
		TalkerStub &_rstub, const Buffer &_rbuf,
		const char *&_bpos, int &_blen, int _firstblen
	);
	void doParseBuffer(TalkerStub &_rstub, const Buffer &_rbuf);
	
	int doExecuteRelayInit(TalkerStub &_rstub);
	int doExecuteConnecting(TalkerStub &_rstub);
	int doExecuteRelayConnecting(TalkerStub &_rstub);
	int doExecuteAccepting(TalkerStub &_rstub);
	int doExecuteRelayAccepting(TalkerStub &_rstub);
	int doExecuteConnected(TalkerStub &_rstub);
	int doExecuteConnectedLimited(TalkerStub &_rstub);
	int doTrySendUpdates(TalkerStub &_rstub);
	int doExecuteDisconnecting(TalkerStub &_rstub);
	int doExecuteDummy(TalkerStub &_rstub);
	void doFillSendBuffer(TalkerStub &_rstub, const uint32 _bufidx);
	void doTryScheduleKeepAlive(TalkerStub &_rstub);
	bool doDummyPushSentBuffer(
		TalkerStub &_rstub,
		uint32 _id,
		const char *_data,
		const uint16 _size
	);
	void doCompleteConnect(TalkerStub &_rstub);
private:
	struct Data;
	struct DataDummy;
	struct DataDirect4;
	struct DataDirect6;
	struct DataRelayed44;
	Data	&d;
};

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
