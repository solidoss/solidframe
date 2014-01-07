// frame/ipc/src/ipcnode.cpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
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
		ConnectedState,
		DisconnectState,
		ForceDisconnectState,
		DisconnectedState
	};
	SessionStub():state(DisconnectedState), sockidx(0xffff), uid(0), remoterelayid(0xffffffff), timestamp_s(0){}
	
	pair<size_t, bool> findPacketId(uint32 _pkgid)const;
	void erasePacketId(uint32 _pkgid);
	
	void clear(){
		state = DisconnectedState;
		sockidx = 0xffff;
		address.clear();
		pairaddr.clear();
		time.seconds(0);
		time.nanoSeconds(0);
		pkgidvec.clear();
		timestamp_s = 0;
		localrelayid = 0;
		remoterelayid = 0xffffffff;
		++uid;
	}
	
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
		sessmap.clear();
		
		const uint	rbufidx = Specific::sizeToIndex(ConnectionStub::ReadBufferCapacity);
	
		if(preadbuf){
			Specific::pushBuffer(preadbuf, rbufidx);
		}
	
		while(sendq.size()){
			char 	*pb = const_cast<char*>(sendq.front().pbuf);
			
			Specific::pushBuffer(pb, sendq.front().bufid);
			sendq.pop();
		}
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
	):nodeid(_nodeid), rservice(_rservice), pendingreadbuffer(NULL), nextsessionidx(0),
	crtsessiondisconnectcnt(0){}
	
	const uint16			nodeid;
	Service					&rservice;
	char					*pendingreadbuffer;
	uint32					nextsessionidx;
	size_t					crtsessiondisconnectcnt;
	
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
	const uint	rbufidx = Specific::sizeToIndex(ConnectionStub::ReadBufferCapacity);
	vdbgx(Debug::ipc, (void*)this<<" "<<rbufidx);
	//free buffers to be sent on UDP
	
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
	
	if(d.pendingreadbuffer){
		Specific::pushBuffer(d.pendingreadbuffer, Specific::capacityToIndex(Packet::Capacity));
	}
	
	delete &d;
}
//--------------------------------------------------------------------
void Node::execute(ExecuteContext &_rexectx){
	Manager		&rm = d.rservice.manager();
	ulong		sig = _rexectx.eventMask();
	{
		const ulong		sm = grabSignalMask();
		if(sm || d.crtsessiondisconnectcnt){
			if(sm & frame::S_KILL){
				idbgx(Debug::ipc, "node - dying");
				_rexectx.close();
				return;
			}
			
			idbgx(Debug::ipc, "node - signaled");
			if(sm == frame::S_RAISE){
				sig |= frame::EventSignal;
				Locker<Mutex>	lock(rm.mutex(*this));
				
				if(d.newconnectionvec.size()){
					doInsertNewConnections();
				}
				
				if(d.newsessionvec.size()){
					doPrepareInsertNewSessions();
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
	
	if(d.crtsessiondisconnectcnt){
		d.rservice.disconnectNodeSessions(*this);
	}
	
	bool			must_reenter(false);
	const AsyncE	rv = doReceiveDatagramPackets(16, socketEvents(0));
	if(rv == AsyncSuccess){
		must_reenter = true;
	}else if(rv == AsyncError){
		_rexectx.close();
		return;
	}
	{
		uint	sigcnt = signaledSize();
		while(sigcnt--){
			const uint	sockidx = signaledFront();
			ulong 		evs = socketEvents(sockidx);
			signaledPop();
			if(sockidx){//skip the udp socket
				doHandleSocketEvents(sockidx, evs);
			}
		}
	}
	
	doSendDatagramPackets();
	
	{
		while(d.timeq.isHit(_rexectx.currentTime())){
			const uint16	idx = d.timeq.frontValue().index;
			const uint16	uid = d.timeq.frontValue().uid;
			
			vdbgx(Debug::ipc, (void*)this<<" timerhit = "<<d.timeq.frontTime().seconds()<<" sesidx = "<<idx);
			
			d.timeq.pop();
			
			SessionStub		&rss = d.sessionvec[idx];
			if(rss.uid == uid && rss.time <= _rexectx.currentTime()){
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
	vdbgx(Debug::ipc, (void*)this<<" reenter = "<<must_reenter<<" udpsendqsz = "<<d.udpsendq.size()<<" signaledsz = "<<signaledSize()<<" tmqsz = "<<d.timeq.size());
	if(must_reenter){
		_rexectx.reschedule();
	}else if(d.timeq.size()){
		_rexectx.waitUntil(d.timeq.frontTime());
	}
}
//--------------------------------------------------------------------
void Node::disconnectSessions(){
	Manager			&rm = d.rservice.manager();
	Locker<Mutex>	lock(rm.mutex(*this));
	
	if(d.newsessionvec.size()){
		doPrepareInsertNewSessions();
	}
	if(d.newsessiontmpvec.size()){
		doInsertNewSessions();
	}
	
	for(SessionVectorT::iterator it(d.sessionvec.begin()); it != d.sessionvec.end(); ++it){
		if(it->state == SessionStub::DisconnectState || it->state == SessionStub::ForceDisconnectState){
			d.rservice.disconnectSession(it->address, it->localrelayid);
			if(it->sockidx != 0xffff){
				ConnectionStub &rcs = d.connectionvec[it->sockidx];
				rcs.sessmap.erase(it->remoterelayid);
			}
			it->clear();
		}
	}
}
//--------------------------------------------------------------------
uint32 Node::pushSession(const SocketAddress &_rsa, const ConnectData &_rconndata, uint32 _idx){
	vdbgx(Debug::ipc, (void*)this<<" idx = "<<_idx<<" condata = "<<_rconndata);
	uint32 fullidx = _idx;
	uint32 idx = _idx;
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
	
	vdbgx(Debug::ipc, (void*)this<<" idx = "<<idx);
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
AsyncE Node::doReceiveDatagramPackets(uint _atmost, const ulong _sig){
	vdbgx(Debug::ipc, (void*)this);
	if(this->socketHasPendingRecv(0)){
		return AsyncWait;
	}
	if(_sig & frame::EventDoneRecv){
		doDispatchReceivedDatagramPacket(d.pendingreadbuffer, socketRecvSize(0), socketRecvAddr(0));
		d.pendingreadbuffer = NULL;
	}
	while(_atmost--){
		char 			*pbuf(Packet::allocate());
		const uint32	bufsz(Packet::Capacity);
		switch(socketRecvFrom(0, pbuf, bufsz)){
			case aio::AsyncSuccess:
				doDispatchReceivedDatagramPacket(pbuf, socketRecvSize(0), socketRecvAddr(0));
				break;
			case aio::AsyncWait:
				d.pendingreadbuffer = pbuf;
				return AsyncWait;
			case aio::AsyncError:
				Packet::deallocate(pbuf);
				return AsyncError;
		}
	}
	return AsyncSuccess;//can still read from socket
}
//--------------------------------------------------------------------
void Node::doSendDatagramPackets(){
	vdbgx(Debug::ipc, (void*)this);
	if(this->socketHasPendingSend(0)){
		return;
	}
	const uint32 evs = socketEvents(0);
	if(evs & frame::EventDoneSend){
		doDoneSendDatagram();
	}
	
	while(d.udpsendq.size()){
		if(d.udpsendq.front().sesuid != d.sessionvec[d.udpsendq.front().sesidx].uid){
			doDoneSendDatagram();
			continue;
		}
		SessionStub			&rss = d.sessionvec[d.udpsendq.front().sesidx];
		const aio::AsyncE 	rv = socketSendTo(
			0, d.udpsendq.front().pbuf,
			d.udpsendq.front().bufsz,
			rss.pairaddr
 		);
		switch(rv){
			case aio::AsyncSuccess:
				doDoneSendDatagram();
				break;
			case aio::AsyncWait:
				return;
			case aio::AsyncError:
				doDoneSendDatagram();
				break;
		}
	}
}
//--------------------------------------------------------------------
void Node::doDoneSendDatagram(){
	ConnectionStub	&rcs = d.connectionvec[d.udpsendq.front().sockid];
	--rcs.readbufusecnt;
	socketPostEvents(d.udpsendq.front().sockid, EventReschedule);
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
		vdbgx(Debug::ipc, (void*)this<<" "<<sesidx<<" "<<sesuid<<" "<<d.sessionvec.size()<<" "<<d.sessionvec[sesidx].uid);
		return;
	}
	SessionStub			&rss = d.sessionvec[sesidx];
	SocketAddressInet	sai(_rsap);
	if(rss.address == sai){
	}else{
		//silently ignore the packet
		//we cannot send back any packet because we don't know the senders relayid
		vdbgx(Debug::ipc, (void*)this<<" wrong sender address");
		return;
	}
	pair<size_t, bool>	rv =rss.findPacketId(p.id());
	if(rv.second){
		//the buffer is already in sent queue
		vdbgx(Debug::ipc, (void*)this<<" buffer already in the sent queue");
		return;
	}
	switch(p.type()){
		default:{
			
		}break;
		case Packet::AcceptType:{
			SessionStub	&rss = d.sessionvec[sesidx];
			AcceptData			accdata;
			SocketAddress		sa;
			const bool			rv = Session::parseAcceptPacket(p, accdata, sa);
			if(!rv){
				return;
			}
			rss.localrelayid = accdata.relayid;
			accdata.relayid = p.relay();
			doRefillAcceptPacket(p, accdata);
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
	
	if(rcs.sendq.back().pkgid < Packet::LastPacketId){
		rss.pkgidvec.push_back(rcs.sendq.back().pkgid);
	}
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
			vdbgx(Debug::ipc, (void*)this<<" push back idx = "<<idx);
		}else{
			SessionStub &rss = d.sessionvec[idx];
			
			if(
				rss.timestamp_s != it->connectdata.timestamp_s ||
				rss.timestamp_n != it->connectdata.timestamp_n ||
				rss.localrelayid != it->connectdata.relayid
			){
				vdbgx(Debug::ipc, (void*)this<<" a restarted peer process");
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
				pair<size_t, bool> r = rss.findPacketId(1/*id of connectpacket*/);
				if(r.second){//a connect message is already in the send queue
					vdbgx(Debug::ipc, (void*)this<<" connect message already in the send queue");
					continue;
				}
				vdbgx(Debug::ipc, (void*)this<<" connect message not in the send queue - sesidx = "<<idx);
				cassert(rss.sockidx != 0xffff);
			}
		}
		SessionStub &rss = d.sessionvec[idx];
		
		if(rss.state == SessionStub::DisconnectState){
			rss.state = SessionStub::ConnectedState;
		}
		
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
	pkt.id(1);
	pkt.relay(fullid);
	
	Session::fillConnectPacket(pkt, _rcd);
	
	pkt.relayPacketSizeStore();
	
	ConnectionStub	&rcs = d.connectionvec[rss.sockidx];
	rcs.sendq.push(SendBufferStub());
	rcs.sendq.back().bufid = pktid;
	rcs.sendq.back().bufsz = pkt.bufferSize();
	rcs.sendq.back().pbuf = pkt.buffer();
	rcs.sendq.back().sessionidx = _idx;
	rcs.sendq.back().sessionuid = rss.uid;
	rcs.sendq.back().pkgid = pkt.id();
	
	rss.pkgidvec.push_back(rcs.sendq.back().pkgid);
	
	pkt.release();//prevent the buffer to be deleted
	
	doTrySendSocketBuffers(rss.sockidx);
}
//--------------------------------------------------------------------
uint16 Node::doCreateSocket(const uint32 _netidx){
	vdbgx(Debug::ipc, (void*)this<<" "<<_netidx);
	//TODO: improve the search - there may be multiple connections to the same address
	//
	const uint32 netidx = d.rservice.netId2AddressFind(_netidx);
	for(ConnectionVectorT::iterator it(d.connectionvec.begin()); it != d.connectionvec.end(); ++it){
		if(it->networkidx == netidx){
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
		d.connectionvec[idx].networkidx = netidx;
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
		const aio::AsyncE rv = socketSend(_sockidx, rsbs.pbuf, rsbs.bufsz);
		switch(rv){
			case aio::AsyncSuccess:{
				rss.erasePacketId(rsbs.pkgid);
				char *pbuf = const_cast<char*>(rcs.sendq.front().pbuf);
				Specific::pushBuffer(pbuf, rcs.sendq.front().bufid);
				rcs.sendq.pop();
			}break;
			case aio::AsyncWait:
				return;
			case aio::AsyncError:
				doDisconnectConnection(_sockidx);
				return;
		}
	}
}
//--------------------------------------------------------------------
void Node::doReceiveStreamData(const uint _sockidx){
	vdbgx(Debug::ipc, (void*)this<<' '<<_sockidx);
	if(this->socketHasPendingRecv(_sockidx)){
		return;
	}
	ConnectionStub	&rcs = d.connectionvec[_sockidx];
	vdbgx(Debug::ipc, (void*)this<<" readbufwritepos = "<<rcs.readbufwritepos<<" readbufreadpos = "<<rcs.readbufreadpos<<" readsz = ");
	while((rcs.readbufwritepos - rcs.readbufreadpos) > Packet::MinRelayReadSize){
		uint16 consume = doReceiveStreamPacket(_sockidx);
		vdbgx(Debug::ipc, (void*)this<<" consume "<<consume);
		if(consume == 0) break;
		rcs.readbufreadpos += consume;
	}
	if(doOptimizeReadBuffer(_sockidx)){
		vdbgx(Debug::ipc, (void*)this<<" readbufwritepos = "<<rcs.readbufwritepos<<" readbufreadpos = "<<rcs.readbufreadpos);
		const aio::AsyncE rv = socketRecv(_sockidx, rcs.preadbuf + rcs.readbufwritepos, rcs.readbufcp - rcs.readbufwritepos);
		switch(rv){
			case aio::AsyncSuccess:
				vdbgx(Debug::ipc, (void*)this<<" readsz = "<<socketRecvSize(_sockidx));
				rcs.readbufwritepos += socketRecvSize(_sockidx);
				socketPostEvents(_sockidx, EventReschedule);
				break;
			case aio::AsyncWait:
				break;
			case aio::AsyncError:
				doDisconnectConnection(_sockidx);
				break;
		}
	}else{
		vdbgx(Debug::ipc, (void*)this<<' '<<_sockidx);
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
		vdbgx(Debug::ipc, (void*)this<<" STREAM RECEIVED: "<<p);
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
				if(!doReceiveConnectStreamPacket(_sockidx, p, sesidx, sesuid)){
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
					const bool 			rv = Session::parseAcceptPacket(p, accdata, sa);
					if(!rv){
						return psz;
					}
					rss.remoterelayid = accdata.relayid;
					accdata.relayid = p.relay();
					doRefillAcceptPacket(p, accdata);
					vdbgx(Debug::ipc, (void*)this<<" AcceptData: "<<accdata);
				}
				
				p.relay(rss.localrelayid);
				doRescheduleSessionTime(sesidx);
			}break;
		}
		d.udpsendq.push(SendDatagramStub());
		d.udpsendq.back().bufsz = p.bufferSize();
		d.udpsendq.back().pbuf = p.release();
		d.udpsendq.back().sockid = _sockidx;
		d.udpsendq.back().sesidx = sesidx;
		d.udpsendq.back().sesuid = sesuid;
		++rcs.readbufusecnt;
		return psz;
	}else{
		p.release();
		return 0;
	}
}
//--------------------------------------------------------------------
void Node::doRefillAcceptPacket(Packet &_rp, const AcceptData _rad){
	_rp.dataSize(0);
	Session::fillAcceptPacket(_rp, _rad);
}
//--------------------------------------------------------------------
void Node::doRefillConnectPacket(Packet &_rp, const ConnectData _rcd){
	_rp.dataSize(0);
	Session::fillConnectPacket(_rp, _rcd);
}
//--------------------------------------------------------------------
bool Node::doReceiveConnectStreamPacket(const uint _sockidx, Packet &_rp, uint16 &_rsesidx, uint16 &_rsesuid){
	vdbgx(Debug::ipc, (void*)this<<" "<<_sockidx<<" "<<_rp);
	
	ConnectionStub					&rcs = d.connectionvec[_sockidx];
	
	const uint32					remoterelayid = _rp.relay();
	SessionIdMapT::const_iterator	it = rcs.sessmap.find(remoterelayid);
	uint32 							fullid = 0xffffffff;
	uint32 							idx;
	
	ConnectData						cd;
	SocketAddress					sa;
	const bool						rv = Session::parseConnectPacket(_rp, cd, sa);
		
	if(!rv){
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
			_rsesidx = it->second;
			_rsesuid = rss.uid;
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
		rcs.sessmap[remoterelayid] = idx;
	}
	
	SessionStub		&rss(d.sessionvec[idx]);
		
	rss.localrelayid = 0xffff;
	
	if(rss.remoterelayid != 0xffffffff){
		rcs.sessmap.erase(rss.remoterelayid);
	}
	
	rss.remoterelayid = remoterelayid;
	rss.address = cd.receiveraddress;
	rss.pairaddr = rss.address;
	rss.timestamp_s = cd.timestamp_s;
	rss.timestamp_n = cd.timestamp_n;
	rss.sockidx = _sockidx;
	_rp.relay(fullid);
	cd.relayid = fullid;
	
	doRefillConnectPacket(_rp, cd);
	
	_rsesidx = idx;
	_rsesuid = rss.uid;
	
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
		if((rcs.readbufcp - rcs.readbufwritepos) >= Packet::Capacity){
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
void Node::doDisconnectConnection(const uint _sockidx){
	vdbgx(Debug::ipc, (void*)this<<" "<<_sockidx<<" disconnected socket");
	ConnectionStub	&rcs = d.connectionvec[_sockidx];
	//close all associated sessions
	for(SessionVectorT::iterator it(d.sessionvec.begin()); it != d.sessionvec.end(); ++it){
		if(it->sockidx == _sockidx){
			it->sockidx = 0xffff;
			it->state = SessionStub::ForceDisconnectState;
			++it->uid; //prevent using this session
			++d.crtsessiondisconnectcnt;
		}
	}
	rcs.clear();
}
//--------------------------------------------------------------------
void Node::doOnSessionTimer(const uint _sesidx){
	vdbgx(Debug::ipc, (void*)this<<' '<<_sesidx<<" disconnecting");
	SessionStub	&rss = d.sessionvec[_sesidx];
	rss.state = SessionStub::DisconnectState;
	++d.crtsessiondisconnectcnt;
}
//--------------------------------------------------------------------
void Node::doHandleSocketEvents(const uint _sockidx, ulong _evs){
	vdbgx(Debug::ipc, (void*)this<<" "<<_sockidx<<" "<<_evs);
	ConnectionStub	&rcs = d.connectionvec[_sockidx];
	if(rcs.state == ConnectionStub::ConnectedState){
		vdbgx(Debug::ipc, (void*)this<<" "<<_sockidx<<" connected state");
		if(_evs & EventDoneRecv){
			rcs.readbufwritepos += socketRecvSize(_sockidx);
			doReceiveStreamData(_sockidx);
		}else if(_evs & EventReschedule){
			doReceiveStreamData(_sockidx);
		}
		if(_evs & EventDoneSend){
			cassert(rcs.sendq.size());
			char *pbuf = const_cast<char*>(rcs.sendq.front().pbuf);
			Specific::pushBuffer(pbuf, rcs.sendq.front().bufid);
			rcs.sendq.pop();
			doTrySendSocketBuffers(_sockidx);
		}
	}else if(rcs.state == ConnectionStub::ConnectState){
		vdbgx(Debug::ipc, (void*)this<<" "<<_sockidx<<" connect to "<<d.rservice.netId2AddressAt(rcs.networkidx).address);
		const aio::AsyncE rv = socketConnect(_sockidx, d.rservice.netId2AddressAt(rcs.networkidx).address);
		switch(rv){
			case aio::AsyncSuccess:
				this->socketPostEvents(_sockidx, 0);
				rcs.state = ConnectionStub::InitState;
				break;
			case aio::AsyncWait:
				rcs.state = ConnectionStub::ConnectWaitState;
				break;
			case aio::AsyncError:
				doDisconnectConnection(_sockidx);
				break;
		}
	}else if(rcs.state == ConnectionStub::ConnectWaitState){
		vdbgx(Debug::ipc, (void*)this<<" "<<_sockidx<<" connected wait state");
		if(_evs & EventDoneIO){
			rcs.state = ConnectionStub::InitState;
			doHandleSocketEvents(_sockidx, _evs);
		}else{
			doDisconnectConnection(_sockidx);
		}
	}else if(rcs.state == ConnectionStub::InitState){
		vdbgx(Debug::ipc, (void*)this<<" "<<_sockidx<<" init state");
		if(!rcs.preadbuf){
			const uint bufidx = Specific::sizeToIndex(ConnectionStub::ReadBufferCapacity);
			rcs.readbufcp = Specific::indexToCapacity(bufidx);
			rcs.preadbuf = Specific::popBuffer(bufidx);
		}
		rcs.state = ConnectionStub::ConnectedState;
		//initiate read
		socketPostEvents(_sockidx, EventReschedule);
		doTrySendSocketBuffers(_sockidx);
	}else{
		cassert(false);
	}
	
}
//--------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
