// frame/ipc/src/ipcsession.hpp
//
// Copyright (c) 2010 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
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
	Context(Service &_srv, const uint16 _tkridx, ObjectUidT const &_robjuid);
	~Context();
	ConnectionContext msgctx;
};

class Session{
public:
	
	static void init();
	static bool parseAcceptPacket(
		const Packet &_rpkt,
		AcceptData &_raccdata,
		const SocketAddress &_rfromsa
	);
	static bool parseConnectPacket(
		const Packet &_rpkt,
		ConnectData &_rconndata,
		const SocketAddress &_rfromsa
	);
	static bool parseErrorPacket(
		const Packet &_rpkt,
		ErrorData &_rdata
	);
	
	static bool fillAcceptPacket(
		Packet &_rpkt,
		const AcceptData &_raccdata
	);
	static bool fillConnectPacket(
		Packet &_rpkt,
		const ConnectData &_rconndata
	);
	static bool fillErrorPacket(
		Packet &_rpkt,
		const ErrorData &_rdata
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
	
	bool preprocessReceivedPacket(
		Packet &_rpkt,
		TalkerStub &_rstub
	);
	
	bool pushReceivedPacket(
		Packet &_rpkt,
		TalkerStub &_rstub
	);
	
	bool pushReceivedErrorPacket(
		Packet &_rpkt,
		TalkerStub &_rstub
	);
	
	void completeConnect(TalkerStub &_rstub, uint16 _port);
	void completeConnect(TalkerStub &_rstub, uint16 _port, uint32 _relayid);
	
	bool executeTimeout(
		TalkerStub &_rstub,
		uint32 _id
	);
	
	AsyncE execute(TalkerStub &_rstub);
	
	bool pushSentPacket(
		TalkerStub &_rstub,
		uint32 _id,
		const char *_data,
		const uint16 _size
	);
	
	void prepareContext(Context &_rctx);
	
	void dummySendError(TalkerStub &_rstub, const SocketAddress &_rsa, int _error);
	
	ConnectionContext::MessagePointerT* requestMessageSafe(const MessageUid &_rmsguid)const;
private:
	bool doPushExpectedReceivedPacket(
		TalkerStub &_rstub,
		Packet &_rpkt
	);
	bool doPushUnxpectedReceivedPacket(
		TalkerStub &_rstub,
		Packet &_rpkt
	);
	bool doFreeSentPackets(const Packet &_rpkt);
	void doParsePacketDataType(
		TalkerStub &_rstub, const Packet &_rpkt,
		const char *&_bpos, int &_blen, int _firstblen
	);
	void doParsePacket(TalkerStub &_rstub, const Packet &_rpkt);
	
	AsyncE doExecuteRelayInit(TalkerStub &_rstub);
	AsyncE doExecuteConnecting(TalkerStub &_rstub);
	AsyncE doExecuteRelayConnecting(TalkerStub &_rstub);
	AsyncE doExecuteAccepting(TalkerStub &_rstub);
	AsyncE doExecuteRelayAccepting(TalkerStub &_rstub);
	AsyncE doExecuteConnected(TalkerStub &_rstub);
	AsyncE doExecuteConnectedLimited(TalkerStub &_rstub);
	AsyncE doTrySendUpdates(TalkerStub &_rstub);
	AsyncE doExecuteDisconnecting(TalkerStub &_rstub);
	AsyncE doExecuteDummy(TalkerStub &_rstub);
	void doFillSendPacket(TalkerStub &_rstub, const uint32 _bufidx);
	void doTryScheduleKeepAlive(TalkerStub &_rstub);
	bool doDummyPushSentPacket(
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
