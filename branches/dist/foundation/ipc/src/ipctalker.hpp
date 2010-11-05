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

struct TimeSpec;

namespace foundation{

class Visitor;

namespace ipc{

class Service;
struct ConnectionUid;
class Session;

//! A talker for io requests
class Talker: public foundation::aio::SingleObject{
public:
	//! Interface from Talker to Session
	struct TalkerStub{
		bool pushSendBuffer(uint32 _id, const char *_pb, uint32 _bl);
		void pushTimer(uint32 _id, const TimeSpec &_rtimepos);
		const TimeSpec& currentTime()const{
			return crttime;
		}
		int basePort()const;
	private:
		friend class Talker;
		TalkerStub(Talker &_rt, const TimeSpec &_rcrttime):rt(_rt), sessionidx(0), crttime(_rcrttime){}
		Talker			&rt;
		uint16			sessionidx;
		const TimeSpec	&crttime;
	};
	typedef Service							ServiceT;
	typedef foundation::aio::SingleObject	BaseT;
	
	Talker(const SocketDevice &_rsd, Service &_rservice, uint16 _id);
	~Talker();
	int execute(ulong _sig, TimeSpec &_tout);
	int execute();
	int accept(foundation::Visitor &);
	int pushSignal(DynamicPointer<Signal> &_psig, const ConnectionUid &_rconid, uint32 _flags);
	void pushSession(Session *_ps, ConnectionUid &_rconid, bool _exists = false);
	void disconnectSessions();
private:
	int doReceiveBuffers(uint32 _atmost, const ulong _sig);
	bool doProcessReceivedBuffers(const TimeSpec &_rts);
	void doDispatchReceivedBuffer(char *_pbuf, const uint32 _bufsz, const SockAddrPair &_rsap);
	void doInsertNewSessions();
	void doDispatchSignals();
	int doSendBuffers(const ulong _sig, const TimeSpec &_rcrttimepos);
	bool doExecuteSessions(const TimeSpec &_rcrttimepos);
private:
	friend struct TalkerStub;
	struct Data;
	Data &d;
};

}//namespace ipc
}//namespace foundation

#endif

