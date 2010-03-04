/* Implementation file ipctalker.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <queue>
#include <map>
#include <cerrno>
#include <cstring>

#include "system/timespec.hpp"
#include "system/socketaddress.hpp"
#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/specific.hpp"

#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "foundation/manager.hpp"

#include "foundation/ipc/ipcservice.hpp"
#include "foundation/ipc/ipcconnectionuid.hpp"

#include "ipctalker.hpp"
#include "ipcsession.hpp"

namespace fdt = foundation;

namespace foundation{
namespace ipc{

struct Talker::Data{
	struct SignalData{
		SignalData(
			DynamicPointer<Signal> &_psig,
			uint16 _sesidx,
			uint16 _sesuid,
			uint32 _flags
		):	psig(_psig), sesid(_sesidx),
			sesuid(_sesuid), flags(_flags){}
		
		DynamicPointer<Signal>	psig;
		uint16					sesidx;
		uint16					sesuid;
		uint32					flags;
	};

	typedef std::pair<char*, uint16>	BufferPairT;
	typedef std::vector<BufferPairT>	BufferPairVectorT;
	typedef Queue<SignalData>			SignalQueueT;
	
	BufferPairVectorT		receivedbufvec;
	char					*pendingreadbuffer;
	Buffer					crtsendbuf;
	SignalQueueT			sigq;
};

//======================================================================

Talker::Talker(
	const SocketDevice &_rsd,
	Service &_rservice,
	uint16 _id
):BaseT(_rsd), d(*new Data(_rservice, _id)){
}

//----------------------------------------------------------------------
Talker::~Talker(){

}

//----------------------------------------------------------------------
inline bool Talker::inDone(ulong _sig, const TimeSpec &_rts){

}
//this should be called under ipc service's mutex lock
void Talker::disconnectSessions(){

}

//----------------------------------------------------------------------
int Talker::execute(ulong _sig, TimeSpec &_tout){
	Manager &rm = Manager::the();
	idbgx(Dbg::ipc, "this = "<<(void*)this<<" &d = "<<(void*)&d);
	if(signaled() || d.closes.size()){
		Mutex::Locker	lock(rm.mutex(*this));
		ulong			sm = grabSignalMask(0);
		
		if(sm & fdt::S_KILL){
			idbgx(Dbg::ipc, "talker - dying");
			return BAD;
		}
		
		idbgx(Dbg::ipc, "talker - signaled");
		if(sm == fdt::S_RAISE){
			_sig |= fdt::SIGNALED;
		}else{
			idbgx(Dbg::ipc, "unknown signal");
		}
		insertSessions();
		dispatchSignals();
	}
	
	bool	must_reenter(false);
	int		rv;
	
	d.readbufsvec.clear();//try to use an on stack Array
	
	rv = receiveBuffers(_sig, d.receivedbufvec);
	if(rv == OK){
		must_reenter = true;
	}else if(rv == BAD){
		return BAD;
	}
	
	must_reenter ||= processReceivedBuffers(d.receivedbufvec);
	
	if(socketHasPendingSend()){
		return must_reenter ? OK : NOK;
	}
	
	if(_sig & OUTDONE){
		dispatchSentBuffer(d.crtsendbuf);
	}
	
	rv = processSignals(_tout);
	if(rv == OK){
		must_reenter = true;
	}else if(rv == BAD){
		return BAD;
	}
	
	rv = sendScheduledBuffers(_tout);
	if(rv == OK){
		must_reenter = true;
	}else if(rv == BAD){
		return BAD;
	}
	
	return must_reenter ? OK : NOK;
}

//----------------------------------------------------------------------
int Talker::execute(){
	return BAD;
}

//----------------------------------------------------------------------
int Talker::accept(foundation::Visitor &){
	return BAD;
}

//----------------------------------------------------------------------
//The talker's mutex should be locked
//return ok if the talker should be signaled
int Talker::pushSignal(
	DynamicPointer<Signal> &_psig,
	const ConnectionUid &_rconid,
	uint32 _flags
){
	d.sigq.push(Data::SignalData(_psig, _rconid.sessionidx, _rconid.sessioncuid, _flags));
	return d.sigq.size() == 1 ? NOK : OK;
}

//----------------------------------------------------------------------
//The talker's mutex should be locked
//Return's the new process connector's id
void Talker::pushSession(Session *_pses, ConnectionUid &_rconid, bool _exists){
}
//----------------------------------------------------------------------
void Talker::insertSessions(){
}
//----------------------------------------------------------------------
void Talker::dispatchSignals(){
}
//----------------------------------------------------------------------
//dispatch d.rcvbuf
void Talker::dispatchReceivedBuffer(const SockAddrPair &_rsap, const TimeSpec &_rts){

}
//----------------------------------------------------------------------
//process signals to be sent d.cq
//return true if the cq is not empty
int Talker::processSignals(const TimeSpec &_rts){
}

//----------------------------------------------------------------------
bool Talker::dispatchSentBuffer(const TimeSpec &_rts){
}

//----------------------------------------------------------------------
void Talker::optimizeBuffer(Buffer &_rbuf){
	uint id(Specific::sizeToId(_rbuf.bufferSize()));
	uint mid(Specific::capacityToId(4096));
	if(mid > id){
		uint32 datasz = _rbuf.dataSize();//the size
		uint32 buffsz = _rbuf.bufferSize();
		Buffer b(Specific::popBuffer(id), Specific::idToCapacity(id));//create a new smaller buffer
		memcpy(b.buffer(), _rbuf.buffer(), buffsz);//copy to the new buffer
		b.dataSize(datasz);
		char *pb = _rbuf.buffer();
		Specific::pushBuffer(pb, mid);
		_rbuf = b;
	}
}
//======================================================================
}//namespace ipc
}//namesapce foundation


