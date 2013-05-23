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
#include "ipcnode.hpp"
#include "ipctalker.hpp"
#include <vector>
#include "frame/ipc/ipcservice.hpp"
#include "ipcpacket.hpp"
#include "system/socketdevice.hpp"

using namespace std;

namespace solid{
namespace frame{
namespace ipc{

typedef std::vector<const Configuration::RelayAddress*>		RelayAddressPointerVectorT;
typedef RelayAddressPointerVectorT::const_iterator			RelayAddressPointerConstInteratorT;

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
	SessionStub():state(0){}
	uint8	state;
	uint32	localrelayid;
	uint32	remoterelayid;
};

struct ConnectionStub{
	enum{
		InitState,
		RegisterState,
	};
	ConnectionStub():state(InitState){}
	uint8					state;
	bool					secure;
	uint32					networkidx;
	
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
	Uint32StackT			freesessionstk;
	SessionVectorT			sessionvec;
	ConnectionVectorT		connectionvec;
};

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
			//TODO:
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
	uint32 idx = _idx;
	if(idx == 0xffffffff){
		if(d.freesessionstk.size()){
			idx = d.freesessionstk.top();
			d.freesessionstk.pop();
		}else{
			idx = d.nextsessionidx;
			++d.nextsessionidx;
		}
	}
	d.newsessionvec.push_back(NewSessionStub(idx, _rsa, _rconndata));
	return 0;
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
void Node::doInsertNewSessions(){
	
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
			d.connectionvec[idx].state = ConnectionStub::RegisterState;
			socketRequestRegister(idx);
		}
	}
}
//--------------------------------------------------------------------
void Node::doPrepareInsertNewSessions(){
	//we don't want to keep the lock for too long
	d.newsessiontmpvec = d.newsessionvec;
	d.newsessionvec.clear();
	for(NewSessionVectorT::const_iterator it(d.newsessiontmpvec.begin()); it != d.newsessiontmpvec.end(); ++it){
		if(it->idx < d.sessionvec.size() && d.sessionvec[it->idx].state == SessionStub::DeleteState){
			d.sessionvec[it->idx].state = SessionStub::ReinitState;
		}
	}
}
//--------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
