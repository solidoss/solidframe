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

using namespace std;

namespace solid{
namespace frame{
namespace ipc{


struct NewSessionStub{
	NewSessionStub(
		const SocketAddress &_rsa,
		const ConnectData &_rconndata
	):address(_rsa), connectdata(_rconndata){}
	
	SocketAddressInet	address;
	ConnectData			connectdata;
};

typedef std::vector<NewSessionStub> NewSessionVectorT;

struct Node::Data{
	Data(uint16 _nodeid, Service &_rservice):nodeid(_nodeid), rservice(_rservice), pendingreadbuffer(NULL){}
	
	const uint16		nodeid;
	Service				&rservice;
	char				*pendingreadbuffer;
	
	NewSessionVectorT	newsessionvec;
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
					doInsertNewSessions();
				}
			}else{
				idbgx(Debug::ipc, "unknown signal");
			}
		}
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
	d.newsessionvec.push_back(NewSessionStub(_rsa, _rconndata));
	return 0;
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
}//namespace ipc
}//namespace frame
}//namespace solid
