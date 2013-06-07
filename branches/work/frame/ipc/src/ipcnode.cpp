/* Implementation file ipcnode.cpp
	
	Copyright 2013 Valentin Palade 
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

#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/socketdevice.hpp"
#include "system/timespec.hpp"
#include "ipcnode.hpp"
#include "ipctalker.hpp"
#include <vector>
#include "frame/ipc/ipcservice.hpp"
#include "ipcpacket.hpp"
#include "ipcsession.hpp"

#include "utility/timerqueue.hpp"
#include <boost/concept_check.hpp>

#ifdef HAS_CPP11
#include <unordered_map>
#else
#include <map>
#endif



using namespace std;

namespace solid{
namespace frame{
namespace ipc{

struct TimerValue{
	TimerValue(
		const uint16 _index = 0,
		const uint16 _uid = 0,
		const uint8 _what = 0,
		const uint8 _flags = 0
	):index(_index), uid(_uid), what(_what), flags(_flags){}
	uint16	index;
	uint16	uid;
	uint8	what;//either connection or session
	uint8	flags;
};

typedef std::vector<const Configuration::RelayAddress*>		RelayAddressPointerVectorT;
typedef RelayAddressPointerVectorT::const_iterator			RelayAddressPointerConstInteratorT;
typedef std::vector<uint32>									Uint32VectorT;
typedef Stack<std::pair<uint16, uint16> >					Uint16PairVectorT;
typedef TimerQueue<TimerValue>								TimerQueueT;

#ifdef HAS_CPP11
	typedef std::unordered_map<
		uint32,
		uint16
	>	SessionIdMapT;
#else
	typedef std::map<
		uint32,
		uint16
	>	SessionIdMapT;
#endif

struct NewSessionStub{
	NewSessionStub(
		uint32 _idx,
		const SocketAddress &_rsa,
		const ConnectData &_rconndata
	):idx(_idx), address(_rsa), connectdata(_rconndata){}
	
	uint32				idx;
	SocketAddressInet	address;
	ConnectData			connectdata;
};

struct NewConnectionStub{
	NewConnectionStub(
		const SocketDevice &_rsd,
		uint32 _netidx,
		aio::openssl::Context *_pctx,
		bool _secure
	):sd(_rsd), netidx(_netidx), pctx(_pctx), secure(_secure){}
	
	SocketDevice			sd;
	uint32					netidx;
	aio::openssl::Context	*pctx;
	bool					secure;
};

struct SessionStub{
	enum{
		InitState,
		ReinitState,
		DeleteState
	};
	SessionStub():state(0), sockidx(0xffff), uid(0), timestamp_s(0){}
	
	pair<size_t, bool> findPacketId(uint32 _pkgid)const;
	void erasePacketId(uint32 _pkgid);
	
	uint16					state;
	uint16					sockidx;
	uint16					uid;
	uint32					localrelayid;
	uint32					remoterelayid;
	uint32					timestamp_s;
	uint32					timestamp_n;
	SocketAddressStub		pairaddr;
	SocketAddressInet		address;
	Uint32VectorT			pkgidvec;
	TimeSpec				time;
};

struct SendBufferStub{
	SendBufferStub():pbuf(NULL){}
	const char	*pbuf;
	uint16		bufsz;
	uint16		bufid;
	uint16		sessionidx;
	uint16		sessionuid;
	uint32		pkgid;
};

struct SendDatagramStub{
	SendDatagramStub():pbuf(0){}
	const char	*pbuf;
	uint16		bufsz;
	uint16		sockid;
	uint16		sesidx;
	uint16		sesuid;
};

typedef Queue<SendBufferStub>			SendBufferQueueT;
typedef Queue<SendDatagramStub>			SendDatagramQueueT;

struct ConnectionStub{
	enum{
		ConnectState,
		ConnectWaitState,
		InitState,
		ConnectedState,
		FailState,
		
		ReadBufferCapacity = 16 * 1024,
	};
	ConnectionStub(
	):state(FailState), networkidx(0xffffffff), preadbuf(NULL),
	readbufreadpos(0), readbufwritepos(0), readbufusecnt(0), sesusecnt(0){}
	
	void clear(){
		state = FailState;
		networkidx = 0xffffffff;
	}
	
	uint8					state;
	bool					secure;
	uint32					networkidx;
	char					*preadbuf;
	uint16					readbufcp;
	uint16					readbufreadpos;
	uint16					readbufwritepos;
	uint16					readbufusecnt;
	uint16					sesusecnt;
	
	SendBufferQueueT		sendq;
	SessionIdMapT			sessmap;
};

typedef std::vector<NewSessionStub> 	NewSessionVectorT;
typedef std::vector<NewConnectionStub>	NewConnectionVectorT;
typedef Stack<uint32>					Uint32StackT;
typedef std::deque<SessionStub>			SessionVectorT;
typedef std::deque<ConnectionStub>		ConnectionVectorT;


struct Node::Data{
	Data(
		uint16 _nodeid,
		Service &_rservice
	):nodeid(_nodeid), rservice(_rservice), pendingreadbuffer(NULL), nextsessionidx(0){}
	
	const uint16			nodeid;
	Service					&rservice;
	char					*pendingreadbuffer;
	uint32					nextsessionidx;
	
	NewSessionVectorT		newsessionvec;
	NewSessionVectorT		newsessiontmpvec;
	NewConnectionVectorT	newconnectionvec;
	Uint16PairVectorT		freesessionstk;
	SessionVectorT			sessionvec;
	ConnectionVectorT		connectionvec;
	TimerQueueT				timeq;
	SendDatagramQueueT		udpsendq;
	
};

//--------------------------------------------------------------------
//--------------------------------------------------------------------

pair<size_t, bool> SessionStub::findPacketId(const uint32 _pkgid)const{
	switch(pkgidvec.size()){
		case 0:
			return pair<size_t, bool>(0, false);
		case 1:
			if(pkgidvec[0] == _pkgid){
				return pair<size_t, bool>(0, true);
			}else{
				return pair<size_t, bool>(1, false);
			}
		case 2:
			if(pkgidvec[0] == _pkgid){
				return pair<size_t, bool>(0, true);
			}else if(pkgidvec[1] == _pkgid){
				return pair<size_t, bool>(1, true);
			}else{
				return pair<size_t, bool>(2, false);
			}
		case 3:
			if(pkgidvec[0] == _pkgid){
				return pair<size_t, bool>(0, true);
			}else if(pkgidvec[1] == _pkgid){
				return pair<size_t, bool>(1, true);
			}else if(pkgidvec[2] == _pkgid){
				return pair<size_t, bool>(2, true);
			}else{
				return pair<size_t, bool>(3, false);
			}
		case 4:
			if(pkgidvec[0] == _pkgid){
				return pair<size_t, bool>(0, true);
			}else if(pkgidvec[1] == _pkgid){
				return pair<size_t, bool>(1, true);
			}else if(pkgidvec[2] == _pkgid){
				return pair<size_t, bool>(2, true);
			}else if(pkgidvec[3] == _pkgid){
				return pair<size_t, bool>(3, true);
			}else{
				return pair<size_t, bool>(4, false);
			}
		case 5:
			if(pkgidvec[0] == _pkgid){
				return pair<size_t, bool>(0, true);
			}else if(pkgidvec[1] == _pkgid){
				return pair<size_t, bool>(1, true);
			}else if(pkgidvec[2] == _pkgid){
				return pair<size_t, bool>(2, true);
			}else if(pkgidvec[3] == _pkgid){
				return pair<size_t, bool>(3, true);
			}else if(pkgidvec[4] == _pkgid){
				return pair<size_t, bool>(4, true);
			}else{
				return pair<size_t, bool>(5, false);
			}
		case 6:
			if(pkgidvec[0] == _pkgid){
				return pair<size_t, bool>(0, true);
			}else if(pkgidvec[1] == _pkgid){
				return pair<size_t, bool>(1, true);
			}else if(pkgidvec[2] == _pkgid){
				return pair<size_t, bool>(2, true);
			}else if(pkgidvec[3] == _pkgid){
				return pair<size_t, bool>(3, true);
			}else if(pkgidvec[4] == _pkgid){
				return pair<size_t, bool>(4, true);
			}else if(pkgidvec[5] == _pkgid){
				return pair<size_t, bool>(5, true);
			}else{
				return pair<size_t, bool>(6, false);
			}
		case 7:
			if(pkgidvec[0] == _pkgid){
				return pair<size_t, bool>(0, true);
			}else if(pkgidvec[1] == _pkgid){
				return pair<size_t, bool>(1, true);
			}else if(pkgidvec[2] == _pkgid){
				return pair<size_t, bool>(2, true);
			}else if(pkgidvec[3] == _pkgid){
				return pair<size_t, bool>(3, true);
			}else if(pkgidvec[4] == _pkgid){
				return pair<size_t, bool>(4, true);
			}else if(pkgidvec[5] == _pkgid){
				return pair<size_t, bool>(5, true);
			}else if(pkgidvec[6] == _pkgid){
				return pair<size_t, bool>(6, true);
			}else{
				return pair<size_t, bool>(7, false);
			}
		case 8:
			if(pkgidvec[0] == _pkgid){
				return pair<size_t, bool>(0, true);
			}else if(pkgidvec[1] == _pkgid){
				return pair<size_t, bool>(1, true);
			}else if(pkgidvec[2] == _pkgid){
				return pair<size_t, bool>(2, true);
			}else if(pkgidvec[3] == _pkgid){
				return pair<size_t, bool>(3, true);
			}else if(pkgidvec[4] == _pkgid){
				return pair<size_t, bool>(4, true);
			}else if(pkgidvec[5] == _pkgid){
				return pair<size_t, bool>(5, true);
			}else if(pkgidvec[6] == _pkgid){
				return pair<size_t, bool>(6, true);
			}else if(pkgidvec[7] == _pkgid){
				return pair<size_t, bool>(7, true);
			}else{
				return pair<size_t, bool>(8, false);
			}
		default:
			for(size_t i = 0; i < pkgidvec.size(); ++i){
				if(pkgidvec[i] == _pkgid){
					return pair<size_t, bool>(i, true);
				}
			}
	}
	return pair<size_t, bool>(pkgidvec.size(), false);
}

void SessionStub::erasePacketId(const uint32 _pkgid){
	pair<size_t, bool> r = findPacketId(_pkgid);
	if(r.second){
		pkgidvec[r.first] = pkgidvec.back();
		pkgidvec.pop_back();
	}
}
//--------------------------------------------------------------------
//--------------------------------------------------------------------
Node::Node(
	const SocketDevice &_rsd,
	Service &_rservice,
	uint16 _id
):BaseT(_rsd),d(*(new Data(_id, _rservice))){
	vdbgx(Debug::ipc, (void*)this);
}
//--------------------------------------------------------------------
Node::~Node(){
	vdbgx(Debug::ipc, (void*)this);
	//free buffers to be sent on UDP
	const uint	wbufidx = Specific::capacityToIndex(Packet::Capacity);
	while(d.udpsendq.size()){
		char *pb = const_cast<char*>(d.udpsendq.front().pbuf);
		Specific::pushBuffer(pb, wbufidx);
		d.udpsendq.pop();
	}
	
	const uint	rbufidx = Specific::sizeToIndex(ConnectionStub::ReadBufferCapacity);
	
	for(ConnectionVectorT::iterator it(d.connectionvec.begin()); it != d.connectionvec.end(); ++it){
		ConnectionStub		&rcs = *it;
		if(rcs.preadbuf){
			Specific::pushBuffer(rcs.preadbuf, rbufidx);
		}
	
		while(rcs.sendq.size()){
			char 		*pb = const_cast<char*>(rcs.sendq.front().pbuf);
			
			Specific::pushBuffer(pb, rcs.sendq.front().bufid);
			rcs.sendq.pop();
		}
	}
	
	delete &d;
}
//--------------------------------------------------------------------
int Node::execute(ulong _sig, TimeSpec &_time){
	Manager		&rm = d.rservice.manager();
	{
		const ulong		sm = grabSignalMask();
		if(sm){
			if(sm & frame::S_KILL){
				idbgx(Debug::ipc, "node - dying");
				return BAD;
			}
			
			idbgx(Debug::ipc, "node - signaled");
			if(sm == frame::S_RAISE){
				_sig |= frame::SIGNALED;
				Locker<Mutex>	lock(rm.mutex(*this));
			
				if(d.newsessionvec.size()){
					doPrepareInsertNewSessions();
				}
				//TODO: here you should do the delete sessions
				if(d.newconnectionvec.size()){
					doInsertNewConnections();
				}
			}else{
				idbgx(Debug::ipc, "unknown signal");
			}
		}
	}
	
	vdbgx(Debug::ipc, (void*)this<<" "<<d.newsessiontmpvec.size());
	
	if(d.newsessiontmpvec.size()){
		doInsertNewSessions();
	}
	
	bool	must_reenter(false);
	int		rv;
	
	rv = doReceiveDatagramPackets(16, socketEvents(0));
	if(rv == OK){
		must_reenter = true;
	}else if(rv == BAD){
		return BAD;
	}
	
	while(signaledSize()){
		const uint sockidx = signaledFront();
		if(sockidx){//skip the udp socket
			ulong evs = socketEvents(signaledFront());
			doHandleSocketEvents(sockidx, evs);
		}
		signaledPop();
	}
	
	doSendDatagramPackets();
	
	{
		while(d.timeq.isHit(_time)){
			const uint16	idx = d.timeq.frontValue().index;
			const uint16	uid = d.timeq.frontValue().uid;
			d.timeq.pop();
			
			SessionStub		&rss = d.sessionvec[idx];
			if(rss.uid == uid && rss.time <= _time){
				doOnSessionTimer(idx);
			}else{
				d.timeq.push(rss.time, TimerValue(idx, rss.uid));
			}
		}
	}
	if(
		signaledSize() || 
		(d.udpsendq.size() && !this->socketHasPendingSend(0))
	){
		must_reenter = true;
	}
	vdbgx(Debug::ipc, (void*)this<<" reenter = "<<must_reenter<<" udbsendqsz = "<<d.udpsendq.size()<<" signaledsz = "<<signaledSize()<<" tmqsz = "<<d.timeq.size());
	if(must_reenter){
		return OK;
	}else{
		if(d.timeq.size()){
			_time = d.timeq.frontTime();
		}
		return NOK;
	}
}


//--------------------------------------------------------------------
uint32 Node::pushSession(const SocketAddress &_rsa, const ConnectData &_rconndata, uint32 _idx){
	vdbgx(Debug::ipc, (void*)this<<" idx = "<<_idx<<" condata = "<<_rconndata);
	uint32 fullidx = _idx;
	uint32 idx;
	if(_idx == 0xffffffff){
		if(d.freesessionstk.size()){
			idx = d.freesessionstk.top().first;
			fullidx = pack(idx, d.freesessionstk.top().second);
			d.freesessionstk.pop();
		}else{
			idx = static_cast<uint16>(d.nextsessionidx);
			fullidx = pack(idx, 0);
			++d.nextsessionidx;
		}
		if(idx == 0xffff){
			return 0xffffffff;
		}
	}
	//we do not use fullidx for now
	d.newsessionvec.push_back(NewSessionStub(idx, _rsa, _rconndata));
	return idx;
}
//--------------------------------------------------------------------
void Node::pushConnection(
	SocketDevice &_rsd,
	uint32 _netidx,
	aio::openssl::Context *_pctx,
	bool _secure
){
	d.newconnectionvec.push_back(NewConnectionStub(_rsd, _netidx, _pctx, _secure));
}
//----------------------------------------------------------------------
int Node::doReceiveDatagramPackets(uint _atmost, const ulong _sig){
	vdbgx(Debug::ipc, (void*)this);
	if(this->socketHasPendingRecv(0)){
		return NOK;
	}
	if(_sig & frame::INDONE){
		doDispatchReceivedDatagramPacket(d.pendingreadbuffer, socketRecvSize(0), socketRecvAddr(0));
		d.pendingreadbuffer = NULL;
	}
	while(_atmost--){
		char 			*pbuf(Packet::allocate());
		const uint32	bufsz(Packet::Capacity);
		switch(socketRecvFrom(0, pbuf, bufsz)){
			case BAD:
				Packet::deallocate(pbuf);
				return BAD;
			case OK:
				doDispatchReceivedDatagramPacket(pbuf, socketRecvSize(0), socketRecvAddr(0));
				break;
			case NOK:
				d.pendingreadbuffer = pbuf;
				return NOK;
		}
	}
	return OK;//can still read from socket
}
//--------------------------------------------------------------------
void Node::doSendDatagramPackets(){
	vdbgx(Debug::ipc, (void*)this);
	if(this->socketHasPendingSend(0)){
		return;
	}
	const uint32 evs = socketEvents(0);
	if(evs & frame::OUTDONE){
		doDoneSendDatagram();
	}
	
	while(d.udpsendq.size()){
		if(d.udpsendq.front().sesuid != d.sessionvec[d.udpsendq.front().sesidx].uid){
			doDoneSendDatagram();
			continue;
		}
		SessionStub &rss = d.sessionvec[d.udpsendq.front().sesidx];
		const int rv = socketSendTo(
			0, d.udpsendq.front().pbuf,
			d.udpsendq.front().bufsz,
			rss.pairaddr
 		);
		switch(rv){
			case BAD:{
				doDoneSendDatagram();
			}break;
			case OK:{
				doDoneSendDatagram();
			}break;
			case NOK:
				return;
		}
	}
}
//--------------------------------------------------------------------
void Node::doDoneSendDatagram(){
	ConnectionStub	&rcs = d.connectionvec[d.udpsendq.front().sockid];
	--rcs.readbufusecnt;
	socketPostEvents(d.udpsendq.front().sockid, INDONE);
	d.udpsendq.pop();
}
//--------------------------------------------------------------------
void Node::doDispatchReceivedDatagramPacket(
	char *_pbuf,
	const uint32 _bufsz,
	const SocketAddress &_rsap
){
	Packet p(_pbuf, Packet::Capacity);
	p.bufferSize(_bufsz);
	vdbgx(Debug::ipc, (void*)this<<" RECEIVED "<<p);
	uint16	sesidx;
	uint16	sesuid;
	
	if(!p.isRelay()){
		return;
	}
	
	uint32				relayid = p.relay();
			
	unpack(sesidx, sesuid, relayid);
	
	if(
		sesidx >= d.sessionvec.size() ||
		sesuid != d.sessionvec[sesidx].uid
	){
		//silently ignore the packet
		//we cannot send back any packet because we don't know the senders relayid
		return;
	}
	SessionStub			&rss = d.sessionvec[sesidx];
	SocketAddressInet	sai(_rsap);
	if(rss.address == sai){
	}else{
		//silently ignore the packet
		//we cannot send back any packet because we don't know the senders relayid
		return;
	}
		
	switch(p.type()){
		default:{
			
		}break;
		case Packet::AcceptType:{
			SessionStub	&rss = d.sessionvec[sesidx];
			
			{
				AcceptData			accdata;
				SocketAddress		sa;
				int					error = Session::parseAcceptPacket(p, accdata, sa);
				if(error){
					return;
				}
				rss.localrelayid = accdata.relayid;
			}
		}break;
	}
	
	p.relay(rss.remoterelayid);
	ConnectionStub		&rcs = d.connectionvec[rss.sockidx];
	
	doRescheduleSessionTime(sesidx);
	
	rcs.sendq.push(SendBufferStub());
	
	rcs.sendq.back().pkgid = p.id();
	rcs.sendq.back().bufsz = _bufsz;
	rcs.sendq.back().pbuf = p.release();
	rcs.sendq.back().bufid = Specific::capacityToIndex(Packet::Capacity);
	rcs.sendq.back().sessionidx = sesidx;
	rcs.sendq.back().sessionuid = sesuid;
	
	rss.pkgidvec.push_back(rcs.sendq.back().pkgid);
	doTrySendSocketBuffers(rss.sockidx);
}
//--------------------------------------------------------------------
void Node::doInsertNewConnections(){
	vdbgx(Debug::ipc, (void*)this);
	for(NewConnectionVectorT::const_iterator it(d.newconnectionvec.begin()); it != d.newconnectionvec.end(); ++it){
		int idx = this->socketInsert(it->sd);
		if(idx >= 0){
			if(idx >= d.connectionvec.size()){
				d.connectionvec.resize(idx + 1);
			}
			d.connectionvec[idx].networkidx = it->netidx;
			d.connectionvec[idx].secure = it->secure;
			d.connectionvec[idx].state = ConnectionStub::InitState;
			socketRequestRegister(idx);
		}
	}
}
//--------------------------------------------------------------------
void Node::doPrepareInsertNewSessions(){
	vdbgx(Debug::ipc, (void*)this);
	d.newsessiontmpvec = d.newsessionvec;
	d.newsessionvec.clear();
}
//--------------------------------------------------------------------
void Node::doInsertNewSessions(){
	vdbgx(Debug::ipc, (void*)this);
	for(NewSessionVectorT::iterator it(d.newsessiontmpvec.begin()); it != d.newsessiontmpvec.end(); ++it){
		uint16 idx/*,uid*/;
		//unpack(idx, uid, it->idx);
		idx = it->idx;
		
		if(idx >= d.sessionvec.size()){
			d.sessionvec.resize(idx + 1);
			SessionStub &rss = d.sessionvec[idx];
			rss.localrelayid = it->connectdata.relayid;
			rss.remoterelayid = 0xffffffff;
			rss.address = it->address;
			rss.pairaddr = rss.address;
			rss.timestamp_s = it->connectdata.timestamp_s;
			rss.timestamp_n = it->connectdata.timestamp_n;
			rss.sockidx = 0xffff;
			
		}else{
			SessionStub &rss = d.sessionvec[idx];
			
			if(
				rss.timestamp_s != it->connectdata.timestamp_s ||
				rss.timestamp_n != it->connectdata.timestamp_n ||
				rss.localrelayid != it->connectdata.relayid
			){
				//a restarted process or another connection
				++rss.uid;
				rss.localrelayid = it->connectdata.relayid;
				rss.timestamp_s = it->connectdata.timestamp_s;
				rss.timestamp_n = it->connectdata.timestamp_n;
				rss.address = it->address;
				rss.pairaddr = rss.address;
				rss.sockidx = 0xffff;
				rss.pkgidvec.clear();
			}else{
				pair<size_t, bool> r = rss.findPacketId(0);
				if(r.second){//a connect message is already in the send queue
					continue;
				}
				cassert(rss.sockidx != 0xffff);
			}
		}
		SessionStub &rss = d.sessionvec[idx];
		
		if(rss.sockidx == 0xffff){
			rss.time = this->currentTime();
			rss.time.add(d.rservice.configuration().node.timeout);
			
			d.timeq.push(rss.time, TimerValue(idx, rss.uid));
		
			rss.sockidx = doCreateSocket(it->connectdata.receivernetworkid);
			ConnectionStub &rcs = d.connectionvec[rss.sockidx];
			++rcs.sesusecnt;
		}else{
			doRescheduleSessionTime(idx);
		}
		doScheduleSendConnect(idx, it->connectdata);
	}
	d.newsessiontmpvec.clear();
}
//--------------------------------------------------------------------
void Node::doRescheduleSessionTime(const uint _sesidx){
	vdbgx(Debug::ipc, (void*)this<<" "<<_sesidx);
	
	SessionStub		&rss = d.sessionvec[_sesidx];
	const TimeSpec	crttime = this->currentTime();
	if(crttime < rss.time){
		rss.time = crttime;
		rss.time.add(d.rservice.configuration().node.timeout);
		d.timeq.push(rss.time, TimerValue(_sesidx, rss.uid));
	}else{
		rss.time = crttime;//will be put on timeq when the current timer fires
	}
}
//--------------------------------------------------------------------
void Node::doScheduleSendConnect(uint16 _idx, ConnectData &_rcd){
	
	SessionStub		&rss = d.sessionvec[_idx];
	const uint32	fullid = pack(_idx, rss.uid);
	const uint32	pktid(Specific::sizeToIndex(128));
	Packet			pkt(Specific::popBuffer(pktid), Specific::indexToCapacity(pktid));
	
	_rcd.relayid = fullid;
	_rcd.sendernetworkid = d.rservice.configuration().localnetid;
	
	vdbgx(Debug::ipc, (void*)this<<" "<<_idx<<_rcd);
	
	pkt.reset();
	pkt.type(Packet::ConnectType);
	pkt.id(1);//TODO!!! the id of the ConnectPacket
	pkt.relay(fullid);
	
	Session::fillConnectPacket(pkt, _rcd);
	
	ConnectionStub	&rcs = d.connectionvec[rss.sockidx];
	rcs.sendq.push(SendBufferStub());
	rcs.sendq.back().bufid = pktid;
	rcs.sendq.back().bufsz = pkt.bufferSize();
	rcs.sendq.back().pbuf = pkt.buffer();
	rcs.sendq.back().sessionidx = _idx;
	rcs.sendq.back().sessionuid = rss.uid;
	rcs.sendq.back().pkgid = pkt.id();
	
	pkt.release();//prevent the buffer to be deleted
	
	doTrySendSocketBuffers(rss.sockidx);
}
//--------------------------------------------------------------------
uint16 Node::doCreateSocket(const uint32 _netidx){
	vdbgx(Debug::ipc, (void*)this<<" "<<_netidx);
	//TODO: improve the search - there may be multiple connections to the same address
	//
	for(ConnectionVectorT::iterator it(d.connectionvec.begin()); it != d.connectionvec.end(); ++it){
		if(it->networkidx == _netidx){
			return it - d.connectionvec.begin();
		}
	}
	//socket not found, create
	
	SocketDevice sd;
	
	sd.create();
	
	sd.makeNonBlocking();
	
	int idx = socketInsert(sd);
	
	if(idx >= 0){
		if(idx >= d.connectionvec.size()){
			d.connectionvec.resize(idx + 1);
		}
		d.connectionvec[idx].networkidx = _netidx;
		d.connectionvec[idx].secure = false;//TODO
		d.connectionvec[idx].state = ConnectionStub::ConnectState;
		socketRequestRegister(idx);
	}
	return static_cast<uint16>(idx);
}
//--------------------------------------------------------------------
void Node::doTrySendSocketBuffers(const uint _sockidx){
	vdbgx(Debug::ipc, (void*)this<<" "<<_sockidx);
	
	ConnectionStub	&rcs = d.connectionvec[_sockidx];
	
	if(socketHasPendingSend(_sockidx) || rcs.sendq.empty() || rcs.state < ConnectionStub::ConnectedState){
		return;
	}
	while(rcs.sendq.size()){
		SendBufferStub	&rsbs = rcs.sendq.front();
		SessionStub 	&rss = d.sessionvec[rsbs.sessionidx];
		if(rss.uid != rsbs.sessionuid){
			rcs.sendq.pop();
			continue;
		}
		const int rv = socketSend(_sockidx, rsbs.pbuf, rsbs.bufsz);
		switch(rv){
			case BAD:
				doPrepareSocketReconnect(_sockidx);
				return;
			case OK:{
				rss.erasePacketId(rsbs.pkgid);
				char *pbuf = const_cast<char*>(rcs.sendq.front().pbuf);
				Specific::pushBuffer(pbuf, rcs.sendq.front().bufid);
				rcs.sendq.pop();
			}break;
			case NOK:
				return;
		}
	}
}
//--------------------------------------------------------------------
void Node::doReceiveStreamData(const uint _sockidx){
	vdbgx(Debug::ipc, (void*)this<<" "<<_sockidx);
	
	ConnectionStub	&rcs = d.connectionvec[_sockidx];
	const uint32 	readsz = socketRecvSize(_sockidx);
	rcs.readbufwritepos += readsz;
	while((rcs.readbufwritepos - rcs.readbufreadpos) > Packet::MinRelayReadSize){
		uint16 consume = doReceiveStreamPacket(_sockidx);
		if(consume == 0) break;
		rcs.readbufreadpos += consume;
	}
	if(doOptimizeReadBuffer(_sockidx)){
		const int rv = socketRecv(_sockidx, rcs.preadbuf + rcs.readbufwritepos, rcs.readbufcp - rcs.readbufwritepos);
		switch(rv){
			case BAD:
				doPrepareSocketReconnect(_sockidx);
				break;
			case OK:
				socketPostEvents(_sockidx, INDONE);
				break;
			case NOK:
				break;
		}
	}
}
//--------------------------------------------------------------------
uint16 Node::doReceiveStreamPacket(const uint _sockidx){
	vdbgx(Debug::ipc, (void*)this<<" "<<_sockidx);
	
	ConnectionStub	&rcs = d.connectionvec[_sockidx];
	char			*pbuf = rcs.preadbuf + rcs.readbufreadpos;
	uint32			bufsz = rcs.readbufwritepos - rcs.readbufreadpos;
	Packet			p(pbuf, Packet::MinRelayReadSize);
	uint32			psz = p.relayPacketSize();
	if(psz <= bufsz){
		p.bufferSize(psz);
		cassert(p.isRelay());
		uint16	sesidx(0xffff);
		uint16	sesuid(0xffff);
		switch(p.type()){
			default:{
				uint32				relayid = p.relay();
				
				unpack(sesidx, sesuid, relayid);
				
				if(
					sesidx >= d.sessionvec.size() ||
					sesuid != d.sessionvec[sesidx].uid
				){
					//silently ignore the packet
					return psz;
				}
				
				const SessionStub	&rss = d.sessionvec[sesidx];
				p.relay(rss.localrelayid);
				doRescheduleSessionTime(sesidx);
			}break;
			case Packet::ConnectType:{
				if(!doReceiveConnectStreamPacket(_sockidx, p)){
					return psz;
				}
			}break;
			case Packet::AcceptType:{
				uint32				relayid = p.relay();
				
				unpack(sesidx, sesuid, relayid);
				
				if(
					sesidx >= d.sessionvec.size() ||
					sesuid != d.sessionvec[sesidx].uid
				){
					//silently ignore the packet
					return psz;
				}
				
				SessionStub	&rss = d.sessionvec[sesidx];
				
				{
					AcceptData			accdata;
					SocketAddress		sa;
					int					error = Session::parseAcceptPacket(p, accdata, sa);
					if(error){
						return psz;
					}
					rss.remoterelayid = accdata.relayid;
				}
				
				p.relay(rss.localrelayid);
				doRescheduleSessionTime(sesidx);
			}break;
		}
		d.udpsendq.push(SendDatagramStub());
		d.udpsendq.back().pbuf = p.release();
		d.udpsendq.back().sockid = _sockidx;
		d.udpsendq.back().sesidx = sesidx;
		d.udpsendq.back().sesuid = sesuid;
		++rcs.readbufusecnt;
		return psz;
	}else{
		return 0;
	}
}
//--------------------------------------------------------------------
bool Node::doReceiveConnectStreamPacket(const uint _sockidx, Packet &_rp){
	vdbgx(Debug::ipc, (void*)this<<" "<<_sockidx<<" "<<_rp);
	
	ConnectionStub					&rcs = d.connectionvec[_sockidx];
	
	SessionIdMapT::const_iterator	it = rcs.sessmap.find(_rp.relay());
	uint32 							fullid = 0xffffffff;
	uint32 							idx;
	
	ConnectData						cd;
	SocketAddress					sa;
	int								error = Session::parseConnectPacket(_rp, cd, sa);
		
	if(error){
		return false;
	}
	
	if(it != rcs.sessmap.end()){
		SessionStub		&rss = d.sessionvec[it->second];
		
		fullid = pack(it->second, rss.uid);
		
		if(
			rss.timestamp_s == cd.timestamp_s &&
			rss.timestamp_n == cd.timestamp_n
		){
			_rp.relay(fullid);
			return true;
		}
		idx = it->second;
	}else{
		Manager			&rm = d.rservice.manager();
		
		{
			Locker<Mutex>	lock(rm.mutex(*this));
			
			if(d.freesessionstk.size()){
				idx = d.freesessionstk.top().first;
				fullid = pack(idx, d.freesessionstk.top().second);
				d.freesessionstk.pop();
			}else{
				idx = static_cast<uint16>(d.nextsessionidx);
				fullid = pack(idx, 0);
				++d.nextsessionidx;
			}
			
			if(idx == 0xffff){
				return false;
			}
		}
		
		if(idx >= d.sessionvec.size()){
			d.sessionvec.resize(idx + 1);
		}
	}
	
	SessionStub		&rss(d.sessionvec[idx]);
		
	rss.localrelayid = 0xffff;
	rss.remoterelayid = _rp.relay();
	rss.address = cd.receiveraddress;
	rss.pairaddr = rss.address;
	rss.timestamp_s = cd.timestamp_s;
	rss.timestamp_n = cd.timestamp_n;
	rss.sockidx = _sockidx;
	_rp.relay(fullid);
	
	rss.time = this->currentTime();
	rss.time.add(d.rservice.configuration().node.timeout);
		
	d.timeq.push(rss.time, TimerValue(idx, rss.uid));
	
	return true;
}
//--------------------------------------------------------------------
bool Node::doOptimizeReadBuffer(const uint _sockidx){
	vdbgx(Debug::ipc, (void*)this<<" "<<_sockidx);
	
	ConnectionStub	&rcs = d.connectionvec[_sockidx];
	if(rcs.readbufusecnt){
		if((rcs.readbufcp - rcs.readbufwritepos) > Packet::Capacity){
			return true;
		}else{
			return false;
		}
	}else{
		if(rcs.readbufreadpos){
			const uint16 tocopy = rcs.readbufwritepos - rcs.readbufreadpos;
			if(tocopy <= rcs.readbufreadpos){//we need to prevent overlapping to avoid memmove
				memcpy(rcs.preadbuf, rcs.preadbuf + rcs.readbufreadpos, tocopy);
				rcs.readbufwritepos = tocopy;
				rcs.readbufreadpos = 0;
			}
		}
		return true;
	}
}
//--------------------------------------------------------------------
void Node::doPrepareSocketReconnect(const uint _sockidx){
	ConnectionStub	&rcs = d.connectionvec[_sockidx];
	//close all associated sessions
}
//--------------------------------------------------------------------
void Node::doOnSessionTimer(const uint _sesidx){
	vdbgx(Debug::ipc, (void*)this<<" "<<_sesidx);
	//TODO: close the session
}
//--------------------------------------------------------------------
void Node::doHandleSocketEvents(const uint _sockidx, ulong _evs){
	vdbgx(Debug::ipc, (void*)this<<" "<<_sockidx<<" "<<_evs);
	ConnectionStub	&rcs = d.connectionvec[_sockidx];
	if(rcs.state == ConnectionStub::ConnectedState){
		if(_evs & INDONE){
			doReceiveStreamData(_sockidx);
		}
		if(_evs & OUTDONE){
			cassert(rcs.sendq.size());
			char *pbuf = const_cast<char*>(rcs.sendq.front().pbuf);
			Specific::pushBuffer(pbuf, rcs.sendq.front().bufid);
			rcs.sendq.pop();
			doTrySendSocketBuffers(_sockidx);
		}
	}else if(rcs.state == ConnectionStub::ConnectState){
		const int rv = socketConnect(_sockidx, d.rservice.netId2AddressAt(rcs.networkidx).address);
		switch(rv){
			case BAD:
				doPrepareSocketReconnect(_sockidx);
				break;
			case OK:
				this->socketPostEvents(_sockidx, 0);
				rcs.state = ConnectionStub::InitState;
				break;
			case NOK:
				rcs.state = ConnectionStub::ConnectWaitState;
				break;
		}
	}else if(rcs.state == ConnectionStub::ConnectWaitState){
		if(_evs & IODONE){
			rcs.state = ConnectionStub::InitState;
			doHandleSocketEvents(_sockidx, _evs);
		}else{
			doPrepareSocketReconnect(_sockidx);
		}
	}else if(rcs.state == ConnectionStub::InitState){
		if(!rcs.preadbuf){
			const uint bufidx = Specific::sizeToIndex(ConnectionStub::ReadBufferCapacity);
			rcs.readbufcp = Specific::indexToCapacity(bufidx);
			rcs.preadbuf = Specific::popBuffer(bufidx);
		}
		rcs.state = ConnectionStub::ConnectedState;
		//initiate read
		socketPostEvents(_sockidx, INDONE);
		doTrySendSocketBuffers(_sockidx);
	}else{
		cassert(false);
	}
	
}
//--------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
