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
		):	psig(_psig), sessionidx(_sesidx),
			sessionuid(_sesuid), flags(_flags){}
		
		DynamicPointer<Signal>	psig;
		uint16					sessionidx;
		uint16					sessionuid;
		uint32					flags;
	};
	struct RecvBuffer{
		RecvBuffer(
			char *_data = NULL,
			uint16 _size = 0,
			uint16 _sessionidx = 0
		):data(_data), size(_size), sessionidx(_sessionidx){}
		char	*data;
		uint16	size;
		uint16	sessionidx;
	};
	struct SessionStub{
		SessionStub(
			Session *_psession = NULL,
			uint16 _uid = 0
		):psession(_psession), uid(_uid), inexeq(false){}
		Session		*psession;
		uint16		uid;
		bool		inexeq;
	};
	typedef std::vector<RecvBuffer>				RecvBufferVectorT;
	typedef Queue<SignalData>					SignalQueueT;
	typedef std::pair<uint16, uint16>			UInt16PairT;
	typedef Stack<UInt16PairT>					UInt16PairStackT;
	typedef std::pair<Session*, uint16>			SessionPairT;
	typedef std::vector<SessionPairT>			SessionPairVectorT;
	typedef std::vector<uint16>					UInt16VectorT;
	typedef std::vector<SessionStub>			SessionStubVectorT;
	typedef Queue<uint16>						UInt16QueueT;
	
	typedef std::map<
		const Inet4SockAddrPair*,
		uint32,
		Inet4AddrPtrCmp
	>											PeerAddr4MapT;//TODO: test if a hash map is better
	typedef std::pair<
		const Inet4SockAddrPair*,
		int
	>											BaseAddr4;
	typedef std::map<
		const BaseAddr4*,
		uint32,
		Inet4AddrPtrCmp
	>											BaseAddr4MapT;
	
	typedef std::map<
		const Inet6SockAddrPair*, 
		uint32,
		Inet6AddrPtrCmp
	>											PeerAddr6MapT;//TODO: test if a hash map is better
	typedef std::pair<
		const Inet6SockAddrPair*,
		int
	>											BaseAddr6;
	typedef std::map<
		const BaseAddr6*,
		uint32, Inet6AddrPtrCmp
	>											BaseAddr6MapT;
public:
	Data(
		Service &_rservice,
		uint16 _id
	):	rservice(_rservice), tkrid(_id), pendingreadbuffer(NULL), nextsessionidx(0){
	}

public:
	Service					&rservice;
	uint16					tkrid;
	RecvBufferVectorT		receivedbufvec;
	char					*pendingreadbuffer;
	Buffer					crtsendbuf;
	SignalQueueT			sigq;
	UInt16PairStackT		freesessionstack;
	uint16					nextsessionidx;
	SessionPairVectorT		newsessionvec;
	UInt16VectorT			closingsessionvec;
	SessionStubVectorT		sessionvec;
	UInt16QueueT			sessionexecq;
	PeerAddr4MapT			peeraddr4map;
	BaseAddr4MapT			baseaddr4map;
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
int Talker::execute(ulong _sig, TimeSpec &_tout){
	Manager &rm = Manager::the();
	idbgx(Dbg::ipc, "this = "<<(void*)this<<" &d = "<<(void*)&d);
	if(signaled() || d.closingsessionvec.size()){
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
		if(d.newsessionvec.size()){
			insertNewSessions();
		}
		if(d.sigq.size()){
			dispatchSignals();
		}
	}
	
	_sig |= socketEventsGrab();
	
	if(_sig & fdt::ERRDONE){
		return BAD;
	}
	
	bool	must_reenter(false);
	int		rv;
	
	if(d.closingsessionvec.size()){
		//this is to ensure the locking order: first service then talker
		d.rservice.disconnectTalkerSessions(*this);
	}
	
	rv = receiveBuffers(_sig, _tout);
	if(rv == OK){
		must_reenter = true;
	}else if(rv == BAD){
		return BAD;
	}
	
	must_reenter = processReceivedBuffers(_tout) || must_reenter;
	
	if(socketHasPendingSend()){
		return must_reenter ? OK : NOK;
	}
	
// 	if(_sig & OUTDONE){
// 		dispatchSentBuffer(d.crtsendbuf);
// 	}
// 	
// 	rv = executeSessions(_tout);
// 	if(rv == OK){
// 		must_reenter = true;
// 	}else if(rv == BAD){
// 		return BAD;
// 	}
// 	
// 	rv = sendScheduledBuffers(_sig, _tout);
// 	if(rv == OK){
// 		must_reenter = true;
// 	}else if(rv == BAD){
// 		return BAD;
// 	}
	
	return must_reenter ? OK : NOK;
}

//----------------------------------------------------------------------
int Talker::receiveBuffers(uint32 _atmost, const ulong _sig){
	if(this->socketHasPendingRecv()){
		return NOK;
	}
	if(_sig & fdt::OUTDONE){
		dispatchReceivedBuffer(d.pendingreadbuffer, socketRecvSize(), socketRecvAddr());
		d.pendingreadbuffer = NULL;
	}
	while(_atmost--){
		char 			*pbuf(Buffer::allocateDataForReading());
		const uint32	bufsz(Buffer::capacityForReading());
		switch(socketRecvFrom(pbuf, bufsz)){
			case BAD:
				Buffer::deallocateDataForReading(pbuf);
				return BAD;
			case OK:
				dispatchReceivedBuffer(pbuf, socketRecvSize(), socketRecvAddr());
				break;
			case NOK:
				d.pendingreadbuffer = pbuf;
				return NOK;
		}
	}
	return OK;//can still read from socket
}
//----------------------------------------------------------------------
bool Talker::processReceivedBuffers(const TimeSpec &_rts){
	for(Data::RecvBufferVectorT::const_iterator it(d.receivedbufvec.begin()); it != d.receivedbufvec.end(); ++it){
		const Data::RecvBuffer	&rcvbuf(*it);
		Data::SessionStub		&rss(d.sessionvec[rcvbuf.sessionidx]);
		Buffer					buf(rcvbuf.data, Buffer::capacityForReading(), rcvbuf.size);
		
		if(rss.psession->pushReceivedBuffer(buf, _rts)){
			if(!rss.inexeq){
				d.sessionexecq.push(rcvbuf.sessionidx);
				rss.inexeq = true;
			}
		}
	}
	d.receivedbufvec.clear();
	return false;
}
//----------------------------------------------------------------------
void Talker::dispatchReceivedBuffer(char *_pbuf, const uint32 _bufsz, const SockAddrPair &_rsap){
	Buffer buf(_pbuf, Buffer::ReadCapacity, _bufsz);
	switch(buf.type()){
		case Buffer::KeepAliveType:
		case Buffer::DataType:{
			
			idbgx(Dbg::ipc, "data buffer");
			Inet4SockAddrPair				inaddr(_rsap);
			Data::PeerAddr4MapT::iterator	pit(d.peeraddr4map.find(&inaddr));
			if(pit != d.peeraddr4map.end()){
				idbgx(Dbg::ipc, "found session for buffer");
				d.receivedbufvec.push_back(Data::RecvBuffer(_pbuf, _bufsz, pit->second));
				buf.release();
			}else{
				//proc
				Buffer::deallocateDataForReading(buf.release());
			}
		
		}break;
		
		case Buffer::ConnectingType:{
			
			Inet4SockAddrPair	inaddr(_rsap);
			int					baseport(Session::parseConnectingBuffer(buf));
			
			idbgx(Dbg::ipc, "connecting buffer with baseport "<<baseport);
			
			if(baseport >= 0){
				Session *ps(new Session(inaddr, baseport, d.rservice.keepAliveTimeout()));
				if(d.rservice.acceptSession(ps)){	
					delete ps;
				}
			}
			
			Buffer::deallocateDataForReading(buf.release());
		}break;
		
		case Buffer::AcceptingType:{
			
			int baseport = Session::parseAcceptedBuffer(buf);
			
			idbgx(Dbg::ipc, "accepting buffer with baseport "<<baseport);
			
			if(baseport >= 0){
				
				Inet4SockAddrPair				inaddr(_rsap);
				Data::BaseAddr4					ppa(&inaddr, baseport);
				Data::BaseAddr4MapT::iterator	bit(d.baseaddr4map.find(&ppa));
				if(bit != d.baseaddr4map.end()){
					Data::SessionStub	&rss(d.sessionvec[bit->second]);
					if(rss.psession){
						rss.psession->completeConnect(inaddr.port());
						//register in peer map
						d.peeraddr4map[rss.psession->peerAddr4()] = bit->second;
						//the connector has at least some updates to send
						if(!rss.inexeq){
							d.sessionexecq.push(bit->second);
							rss.inexeq = true;
						}
					}
				}
			}
			
			Buffer::deallocateDataForReading(buf.release());
		}break;
		
		default:
			Buffer::deallocateDataForReading(buf.release());
			cassert(false);
	}
}
//----------------------------------------------------------------------
//The talker's mutex should be locked
//return ok if the talker should be signaled
int Talker::pushSignal(
	DynamicPointer<Signal> &_psig,
	const ConnectionUid &_rconid,
	uint32 _flags
){
	d.sigq.push(Data::SignalData(_psig, _rconid.sessionidx, _rconid.sessionuid, _flags));
	return d.sigq.size() == 1 ? NOK : OK;
}

//----------------------------------------------------------------------
//The talker's mutex should be locked
//Return's the new process connector's id
void Talker::pushSession(Session *_pses, ConnectionUid &_rconid, bool _exists){
	vdbgx(Dbg::ipc, "exists "<<_exists);
	if(_exists){
		++_rconid.sessionuid;
	}else{
		if(d.freesessionstack.size()){
			vdbgx(Dbg::ipc, "");
			_rconid.sessionidx = d.freesessionstack.top().first;
			_rconid.sessionuid = d.freesessionstack.top().second;
			d.freesessionstack.pop();
		}else{
			vdbgx(Dbg::ipc, "");
			cassert(d.nextsessionidx < (uint16)0xffff);
			_rconid.sessionidx = d.nextsessionidx;
			_rconid.sessionuid = 0;
			++d.nextsessionidx;//TODO: it must be reseted somewhere!!!
		}
	}
	d.newsessionvec.push_back(Data::SessionPairT(_pses, _rconid.sessionidx));
}
//----------------------------------------------------------------------
void Talker::insertNewSessions(){
	for(Data::SessionPairVectorT::const_iterator it(d.newsessionvec.begin()); it != d.newsessionvec.end(); ++it){
		vdbgx(Dbg::ipc, "newsession idx = "<<it->second<<" session vector size = "<<d.sessionvec.size());
		
		if(it->second >= d.sessionvec.size()){
			d.sessionvec.resize(it->second + 1);
		}
		Data::SessionStub &rss(d.sessionvec[it->second]);
		if(rss.psession == NULL){
			rss.psession = it->first;
			rss.psession->prepare();
			d.baseaddr4map[rss.psession->baseAddr4()] = it->second;
			if(rss.psession->isConnected()){
				d.peeraddr4map[rss.psession->peerAddr4()] = it->second;
			}
			if(!rss.inexeq){
				d.sessionexecq.push(it->second);
				rss.inexeq = true;
			}
		}else{
			if(!rss.psession->isAccepting()){
				//a reconnect
				d.peeraddr4map.erase(rss.psession->peerAddr4());
				rss.psession->reconnect(it->first);
				++rss.uid;
				d.peeraddr4map[rss.psession->peerAddr4()] = it->second;
				if(!rss.inexeq){
					d.sessionexecq.push(it->second);
					rss.inexeq = true;
				}
			}
			delete it->first;
		}
	}
	d.newsessionvec.clear();
	d.nextsessionidx = d.sessionvec.size();
}
//----------------------------------------------------------------------
void Talker::dispatchSignals(){
	//dispatch signals before disconnecting sessions
	while(d.sigq.size()){
		cassert(d.sigq.front().sessionidx < d.sessionvec.size());
		Data::SignalData	&rsd(d.sigq.front());
		Data::SessionStub	&rss(d.sessionvec[rsd.sessionidx]);
		const uint32		flags(rsd.flags);
		
		if(
			rss.psession && 
			(
				!(flags & Service::SameConnectorFlag) ||
				rss.uid == rsd.sessionuid
			)
		){
			rss.psession->pushSignal(rsd.psig, flags);
			if(!rss.inexeq){
				d.sessionexecq.push(rsd.sessionidx);
				rss.inexeq = true;
			}
		}
		
		d.sigq.pop();
	}
}
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//this should be called under ipc service's mutex lock
void Talker::disconnectSessions(){
	Manager 		&rm(Manager::the());
	Mutex::Locker	lock(rm.mutex(*this));
	//delete sessions
	
	for(Data::UInt16VectorT::const_iterator it(d.closingsessionvec.begin()); it != d.closingsessionvec.end(); ++it){
		Data::SessionStub &rss(d.sessionvec[*it]);
		vdbgx(Dbg::ipc, "disconnecting sessions "<<(void*)rss.psession);
		cassert(rss.psession);
		if(rss.psession->isDisconnecting()){
			idbgx(Dbg::ipc, "deleting session "<<(void*)rss.psession<<" on pos "<<*it);
			d.rservice.disconnectSession(rss.psession);
			//unregister from base and peer:
			if(rss.psession->peerAddr4() && rss.psession->peerAddr4()->addr){
				d.peeraddr4map.erase(rss.psession->peerAddr4());
			}
			if(
				rss.psession->baseAddr4() &&
				rss.psession->baseAddr4()->first &&
				rss.psession->baseAddr4()->first->addr
			){
				d.baseaddr4map.erase(rss.psession->baseAddr4());
			}
			delete rss.psession;
			rss.psession = NULL;
			++rss.uid;
			d.freesessionstack.push(Data::UInt16PairT(*it, rss.uid));
		}
	}
	d.closingsessionvec.clear();
}
//----------------------------------------------------------------------
void Talker::optimizeBuffer(Buffer &_rbuf){
	const uint id(Specific::sizeToId(_rbuf.bufferSize()));
	const uint mid(Specific::capacityToId(Buffer::capacityForReading()));
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
//----------------------------------------------------------------------
int Talker::execute(){
	return BAD;
}
//----------------------------------------------------------------------
int Talker::accept(foundation::Visitor &){
	return BAD;
}
//======================================================================
}//namespace ipc
}//namesapce foundation


