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

	void prepare(Talker::TalkerStub &_rstub);
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
		Talker::TalkerStub &_rstub
	);
	
	bool pushReceivedBuffer(
		Buffer &_rbuf,
		Talker::TalkerStub &_rstub
	);
	
	bool pushReceivedErrorBuffer(
		Buffer &_rbuf,
		Talker::TalkerStub &_rstub
	);
	
	void completeConnect(Talker::TalkerStub &_rstub, uint16 _port);
	void completeConnect(Talker::TalkerStub &_rstub, uint16 _port, uint32 _relayid);
	
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
	
	void dummySendError(Talker::TalkerStub &_rstub, const SocketAddress &_rsa, int _error);
private:
	bool doPushExpectedReceivedBuffer(
		Talker::TalkerStub &_rstub,
		Buffer &_rbuf
	);
	bool doPushUnxpectedReceivedBuffer(
		Talker::TalkerStub &_rstub,
		Buffer &_rbuf
	);
	bool doFreeSentBuffers(const Buffer &_rbuf);
	void doParseBufferDataType(
		Talker::TalkerStub &_rstub, const Buffer &_rbuf,
		const char *&_bpos, int &_blen, int _firstblen
	);
	void doParseBuffer(Talker::TalkerStub &_rstub, const Buffer &_rbuf);
	
	int doExecuteRelayInit(Talker::TalkerStub &_rstub);
	int doExecuteConnecting(Talker::TalkerStub &_rstub);
	int doExecuteRelayConnecting(Talker::TalkerStub &_rstub);
	int doExecuteAccepting(Talker::TalkerStub &_rstub);
	int doExecuteRelayAccepting(Talker::TalkerStub &_rstub);
	int doExecuteConnected(Talker::TalkerStub &_rstub);
	int doExecuteConnectedLimited(Talker::TalkerStub &_rstub);
	int doTrySendUpdates(Talker::TalkerStub &_rstub);
	int doExecuteDisconnecting(Talker::TalkerStub &_rstub);
	int doExecuteDummy(Talker::TalkerStub &_rstub);
	void doFillSendBuffer(Talker::TalkerStub &_rstub, const uint32 _bufidx);
	void doTryScheduleKeepAlive(Talker::TalkerStub &_rstub);
	bool doDummyPushSentBuffer(
		Talker::TalkerStub &_rstub,
		uint32 _id,
		const char *_data,
		const uint16 _size
	);
	void doCompleteConnect(Talker::TalkerStub &_rstub);
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
