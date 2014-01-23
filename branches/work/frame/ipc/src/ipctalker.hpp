// frame/ipc/src/ipctalker.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_SRC_IPC_TALKER_HPP
#define SOLID_FRAME_IPC_SRC_IPC_TALKER_HPP

#include "frame/aio/aiosingleobject.hpp"
#include "frame/ipc/ipcconnectionuid.hpp"
#include <ostream>

struct TimeSpec;
namespace solid{
namespace frame{
namespace ipc{

class Service;
struct ConnectionUid;
class Session;

struct ConnectData{
	enum{
		BaseSize = 5 + 1 + 2 + 2 + 2 + 2 + 4 + 4
	};
	enum{
		BasicType = 1,
		Relay4Type = 2,
		Relay6Type = 3
	};
	enum{
		VersionMajor = 1,
		VersionMinor = 0
	};
	
	ConnectData():
		s('s'), f('f'), i('i'), p('p'), c('c'), type(0),
		version_major(VersionMajor), version_minor(VersionMinor),
		flags(0), baseport(0), timestamp_s(0), timestamp_n(0),
		relayid(0), receivernetworkid(0), sendernetworkid(0){}
	
	std::ostream& print(std::ostream& _ros)const;
	uint8					s;
	uint8					f;
	uint8					i;
	uint8					p;
	uint8					c;
	uint8					type;
	uint16					version_major;
	uint16					version_minor;
	uint16					flags;
	uint16					baseport;
	uint32					timestamp_s;
	uint32					timestamp_n;
	//relay
	uint32					relayid;
	uint32					receivernetworkid;
	SocketAddressInet		receiveraddress;
	uint32					sendernetworkid;
	SocketAddressInet		senderaddress;
};

std::ostream& operator<<(std::ostream& _ros, const ConnectData &_rd);

struct AcceptData{
	enum{
		BaseSize = 2 + 2 + 4 + 4
	};
	AcceptData():baseport(0){}
	
	std::ostream& print(std::ostream& _ros)const;
	
	uint16	flags;
	uint16	baseport;
	uint32	timestamp_s;
	uint32	timestamp_n;
	uint32	relayid;
};

struct ErrorData{
	ErrorData():error(0){}
	
	int error;
};

std::ostream& operator<<(std::ostream& _ros, const AcceptData &_rd);

class Talker;

struct TalkerStub{
	bool pushSendPacket(uint32 _id, const char *_pb, uint32 _bl);
	void pushTimer(uint32 _id, const TimeSpec &_rtimepos);
	const TimeSpec& currentTime()const{
		return crttime;
	}
	int basePort()const;
	Service& service()const{
		return rs;
	}
	uint32 relayId()const;
private:
	friend class Talker;
	TalkerStub(
		Talker &_rt,
		Service &_rs,
			const TimeSpec &_rcrttime
	):rt(_rt), rs(_rs), sessionidx(0), crttime(_rcrttime){}
	Talker			&rt;
	Service			&rs;
	uint16			sessionidx;
	const TimeSpec	&crttime;
	
};

//! A talker for io requests
class Talker: public Dynamic<Talker, frame::aio::SingleObject>{
public:
	//! Interface from Talker to Session
	//typedef Service							ServiceT;
	
	Talker(const SocketDevice &_rsd, Service &_rservice, uint16 _tkridx);
	~Talker();
	
	bool pushMessage(
		DynamicPointer<Message> &_pmsgptr,
		const SerializationTypeIdT &_rtid,
		const ConnectionUid &_rconid,
		uint32 _flags
	);
	bool pushEvent(
		const ConnectionUid &_rconid,
		int32 _event,
		uint32 _flags
	);
		
	void pushSession(Session *_ps, ConnectionUid &_rconid, bool _exists = false);
	void disconnectSessions(TalkerStub &_rstub);
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
	AsyncE doReceivePackets(TalkerStub &_rstub, uint32 _atmost, const ulong _sig);
	bool doProcessReceivedPackets(TalkerStub &_rstub);
	bool doPreprocessReceivedPackets(TalkerStub &_rstub);
	void doDispatchReceivedPacket(
		TalkerStub &_rstub,
		char *_pbuf,
		const uint32 _bufsz,
		const SocketAddress &_rsap
	);
	void doInsertNewSessions(TalkerStub &_rstub);
	void doDispatchMessages();
	void doDispatchEvents();
	AsyncE doSendPackets(TalkerStub &_rstub, const ulong _sig);
	bool doExecuteSessions(TalkerStub &_rstub);
private:
	friend struct TalkerStub;
	struct Data;
	Data &d;
};

}//namespace ipc
}//namespace frame
}//namespace solid

#endif

