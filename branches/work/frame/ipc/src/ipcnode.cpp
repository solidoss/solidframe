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

using namespace std;

namespace solid{
namespace frame{
namespace ipc{

typedef std::vector<const Configuration::RelayAddress*>		RelayAddressPointerVectorT;
typedef RelayAddressPointerVectorT::const_iterator			RelayAddressPointerConstInteratorT;
typedef std::vector<uint32>									Uint32VectorT;
typedef Stack<std::pair<uint16, uint16> >					Uint16PairVectorT;

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

typedef Queue<SendBufferStub>			SendBufferQueueT;

struct ConnectionStub{
	enum{
		ConnectState,
		InitState,
		ConnectedState,
		FailState,
	};
	ConnectionStub():state(FailState), networkidx(0xffffffff){}
	uint8					state;
	bool					secure;
	uint32					networkidx;
	SendBufferQueueT		sendq;
	
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
):BaseT(_rsd),d(*(new Data(_id, _rservice))){}
//--------------------------------------------------------------------
Node::~Node(){
	delete &d;
}
//--------------------------------------------------------------------
int Node::execute(ulong _sig, TimeSpec &_tout){
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
	
	if(d.newsessiontmpvec.size()){
		doInsertNewSessions();
	}
	
	bool	must_reenter(false);
	int		rv;
	
	rv = doReceiveDatagramPackets(16, socketEvents(0));
	
	while(signaledSize()){
		const uint sockidx = signaledFront();
		if(sockidx){//skip the udp socket
			ulong evs = socketEvents(signaledFront());
			doHandleSocketEvents(sockidx, evs);
		}
		signaledPop();
	}
	
	if(rv == OK){
		must_reenter = true;
	}else if(rv == BAD){
		return BAD;
	}
	
	return must_reenter ? OK : NOK;
}


//--------------------------------------------------------------------
uint32 Node::pushSession(const SocketAddress &_rsa, const ConnectData &_rconndata, uint32 _idx){
	uint32 fullidx = _idx;
	uint32 idx;
	if(idx == 0xffffffff){
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
void Node::doDispatchReceivedDatagramPacket(
	char *_pbuf,
	const uint32 _bufsz,
	const SocketAddress &_rsap
){
	
}
//--------------------------------------------------------------------
void Node::doInsertNewConnections(){
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
	d.newsessiontmpvec = d.newsessionvec;
	d.newsessionvec.clear();
}
//--------------------------------------------------------------------
void Node::doInsertNewSessions(){
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
			rss.sockidx = doCreateSocket(it->connectdata.receivernetworkid);
		}
		doScheduleSendConnect(idx, it->connectdata);
	}
	d.newsessiontmpvec.clear();
}
//--------------------------------------------------------------------
void Node::doScheduleSendConnect(uint16 _idx, ConnectData &_rcd){
	SessionStub		&rss = d.sessionvec[_idx];
	const uint32	fullid = pack(_idx, rss.uid);
	const uint32	pktid(Specific::sizeToIndex(128));
	Packet			pkt(Specific::popBuffer(pktid), Specific::indexToCapacity(pktid));
	
	_rcd.relayid = fullid;
	pkt.reset();
	pkt.type(Packet::ConnectType);
	pkt.id(1);//TODO!!! the id of the ConnectPacket
	pkt.relay(0xffffffff);
	
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
	//TODO: improve the search
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
	return 0;
}
//--------------------------------------------------------------------
void Node::doTrySendSocketBuffers(const uint _sockidx){
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
			case OK:
				rss.erasePacketId(rsbs.pkgid);
				rcs.sendq.pop();
				break;
			case NOK:
				return;
		}
	}
}
//--------------------------------------------------------------------
void Node::doPrepareSocketReconnect(const uint _sockidx){
	ConnectionStub	&rcs = d.connectionvec[_sockidx];
}
//--------------------------------------------------------------------
void Node::doHandleSocketEvents(const uint _sockidx, ulong _evs){
	ConnectionStub	&rcs = d.connectionvec[_sockidx];
	
}
//--------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
