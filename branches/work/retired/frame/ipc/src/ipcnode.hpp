// frame/ipc/src/ipcnode.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_SRC_IPC_NODE_HPP
#define SOLID_FRAME_IPC_SRC_IPC_NODE_HPP

#include "frame/aio/aiomultiobject.hpp"
#include "frame/ipc/ipcconnectionuid.hpp"

namespace solid{

class SocketAddress;

namespace frame{

namespace aio{
namespace openssl{
class Context;
}
}

namespace ipc{

struct ConnectData;
struct AcceptData;
struct Packet;

class Service;

class Node: public Dynamic<Node, frame::aio::MultiObject>{
public:
	Node(
		const SocketDevice &_rsd,
		Service &_rservice,
		uint16 _id
	);
	~Node();
	
	uint32 pushSession(
		const SocketAddress &_rsa,
		const ConnectData &_rconndata,
		uint32 _idx = 0xffffffff
	);
	void pushConnection(
		SocketDevice &_rsd,
		uint32 _netoff,
		aio::openssl::Context *_pctx,
		bool _secure
	);
	void disconnectSessions();
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
	
	void doInsertNewSessions();
	void doPrepareInsertNewSessions();
	void doInsertNewConnections();
	AsyncE doReceiveDatagramPackets(uint _atmost, const ulong _sig);
	void doDispatchReceivedDatagramPacket(
		char *_pbuf,
		const uint32 _bufsz,
		const SocketAddress &_rsap
	);
	void doScheduleSendConnect(uint16 _idx, ConnectData &_rcd);
	uint16 doCreateSocket(const uint32 _netidx);
	void doTrySendSocketBuffers(const uint _sockidx);
	void doSendDatagramPackets();
	void doReceiveStreamData(const uint _sockidx);
	void doDisconnectConnection(const uint _sockidx);
	void doHandleSocketEvents(const uint _sockidx, ulong _evs);
	uint16 doReceiveStreamPacket(const uint _sockidx);
	bool doOptimizeReadBuffer(const uint _sockidx);
	void doDoneSendDatagram();
	bool doReceiveConnectStreamPacket(const uint _sockidx, Packet &_rp, uint16 &_rsesidx, uint16 &_rsesuid);
	void doRescheduleSessionTime(const uint _sesidx);
	void doOnSessionTimer(const uint _sesidx);
	void doRefillAcceptPacket(Packet &_rp, const AcceptData _rad);
	void doRefillConnectPacket(Packet &_rp, const ConnectData _rcd);
private:
	struct Data;
	Data &d;
};



}//namespace ipc
}//namespace frame
}//namespace solid

#endif
