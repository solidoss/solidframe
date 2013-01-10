/* Declarations file ipctalker.hpp
	
	Copyright 2007, 2008 Valentin Palade 
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

#ifndef FOUNDATION_IPC_SRC_IPC_TALKER_HPP
#define FOUNDATION_IPC_SRC_IPC_TALKER_HPP

#include "foundation/aio/aiosingleobject.hpp"
#include "foundation/ipc/ipcconnectionuid.hpp"

struct TimeSpec;

namespace foundation{

class Visitor;

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

struct AcceptData{
	enum{
		BaseSize = 2 + 2 + 4 + 4
	};
	AcceptData():baseport(0){}
	uint16	flags;
	uint16	baseport;
	uint32	timestamp_s;
	uint32	timestamp_n;
	uint32	relayid;
};

//! A talker for io requests
class Talker: public Dynamic<Talker, foundation::aio::SingleObject>{
public:
	//! Interface from Talker to Session
	struct TalkerStub{
		bool pushSendBuffer(uint32 _id, const char *_pb, uint32 _bl);
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
	typedef Service							ServiceT;
	
	Talker(const SocketDevice &_rsd, Service &_rservice, uint16 _id);
	~Talker();
	int execute(ulong _sig, TimeSpec &_tout);
	int execute();
	int accept(foundation::Visitor &);
	bool pushSignal(
		DynamicPointer<Signal> &_psig,
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
	void disconnectSessions();
private:
	int doReceiveBuffers(TalkerStub &_rstub, uint32 _atmost, const ulong _sig);
	bool doProcessReceivedBuffers(TalkerStub &_rstub);
	void doDispatchReceivedBuffer(
		TalkerStub &_rstub,
		char *_pbuf,
		const uint32 _bufsz,
		const SocketAddress &_rsap
	);
	void doInsertNewSessions(TalkerStub &_rstub);
	void doDispatchSignals();
	void doDispatchEvents();
	int doSendBuffers(TalkerStub &_rstub, const ulong _sig);
	bool doExecuteSessions(TalkerStub &_rstub);
private:
	friend struct TalkerStub;
	struct Data;
	Data &d;
};

}//namespace ipc
}//namespace foundation

#endif

