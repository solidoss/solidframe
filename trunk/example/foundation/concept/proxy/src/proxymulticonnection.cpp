/* Implementation file proxymulticonnection.cpp
	
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

#include "core/manager.hpp"
#include "proxy/proxyservice.hpp"
#include "proxymulticonnection.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/timespec.hpp"

namespace fdt=foundation;
//static const char	*hellostr = "Welcome to proxy service!!!\r\n"; 

namespace concept{

namespace proxy{

MultiConnection::MultiConnection(const char *_node, const char *_srv): 
									//bend(bbeg + BUFSZ),brpos(bbeg),bwpos(bbeg),
									pai(NULL),b(false){
	cassert(false);
}
MultiConnection::MultiConnection(const SocketDevice &_rsd):
	BaseT(_rsd), pai(NULL), b(false)
{
	bp = be = NULL;
	state(READ_ADDR);
}
/*
NOTE: 
* Releasing the connection here and not in a base class destructor because
here we know the exact type of the object - so the service can do other things 
based on the type.
* Also it ensures a proper/safe visiting. Suppose the unregister would have taken 
place in a base destructor. If the visited object is a leaf, one may visit
destroyed data.
NOTE: 
* Visitable data must be destroyed after releasing the connection!!!
*/

MultiConnection::~MultiConnection(){
	delete pai;
}
enum{
		Receive,
		ReceiveWait,
		Send,
		SendWait
	};
int MultiConnection::execute(ulong _sig, TimeSpec &_tout){
	idbg("time.sec "<<_tout.seconds()<<" time.nsec = "<<_tout.nanoSeconds());
	if(_sig & (fdt::TIMEOUT | fdt::ERRDONE | fdt::TIMEOUT_RECV | fdt::TIMEOUT_SEND)){
		idbg("connecton timeout or error : tout "<<fdt::TIMEOUT<<" err "<<fdt::ERRDONE<<" toutrecv "<<fdt::TIMEOUT_RECV<<" tout send "<<fdt::TIMEOUT_SEND);
		//We really need this check as epoll upsets if we register an unconnected socket
		if(state() != CONNECT){
			//lets see which socket has timeout:
			while(this->signaledSize()){
				uint evs = socketEvents(signaledFront());
				idbg("for "<<signaledFront()<<" evs "<<evs);
				this->signaledPop();
			}
			return BAD;
		}
	}
	if(signaled()){
		concept::Manager &rm = concept::Manager::the();
		{
		Mutex::Locker	lock(rm.mutex(*this));
		ulong sm = grabSignalMask();
		if(sm & fdt::S_KILL) return BAD;
		}
	}
	int rv = NOK;
	switch(state()){
		case READ_ADDR:
			idbgx(Dbg::any, "READ_ADDR");
		case READ_PORT:
			idbgx(Dbg::any, "READ_PORT");
			return doReadAddress();
		case REGISTER_CONNECTION:{
			idbgx(Dbg::any, "REGISTER_CONNECTION");
			pai = new AddrInfo(addr.c_str(), port.c_str());
			it = pai->begin();
			SocketDevice	sd;
			sd.create(it);
			sd.makeNonBlocking();
			socketRequestRegister(socketInsert(sd));
			state(CONNECT);
			}return NOK;
		case CONNECT://connect the other end:
			//TODO: check if anything went wrong
			while(this->signaledSize()){
				this->signaledPop();
			}
			idbgx(Dbg::any, "CONNECT");
			switch(socketConnect(1, it)){
				case BAD:
					idbgx(Dbg::any, "BAD");
					return BAD;
				case OK:
					state(SEND_REMAINS);
					idbgx(Dbg::any, "OK");
					return OK;
				case NOK:
					state(CONNECT_TOUT);
					idbgx(Dbg::any, "NOK");
					return NOK;
			};
		case CONNECT_TOUT:{
			idbgx(Dbg::any, "CONNECT_TOUT");
			uint32 evs = socketEvents(1);
			socketState(0, Receive);
			socketState(1, Receive);
			if(!evs || !(evs & fdt::OUTDONE)) return BAD;
			state(SEND_REMAINS);
			}
		case SEND_REMAINS:{
			idbgx(Dbg::any, "SEND_REMAINS");
			state(PROXY);
			if(bp != be){
				switch(socketSend(1, bp, be - bp)){
					case BAD:
						idbgx(Dbg::any, "BAD");
						return BAD;
					case OK:
						idbgx(Dbg::any, "OK");
						break;
					case NOK:
						stubs[0].recvbuf.usecnt = 1;
						idbgx(Dbg::any, "NOK");
						return NOK;
				}
			}
			}
		case PROXY:
			idbgx(Dbg::any, "PROXY");
			rv =  doProxy(_tout);
	}
	while(this->signaledSize()){
		this->signaledPop();
	}
	return rv;
}
int MultiConnection::doReadAddress(){
	if(bp and !be) return doRefill();
	char *bb = bp;
	switch(state()){
		case READ_ADDR:
			idbgx(Dbg::any, "READ_ADDR");
			while(bp != be){
				if(*bp == ' ') break;
				++bp;
			}
			addr.append(bb, bp - bb);
			if(bp == be) return doRefill();
			if(addr.size() > 64) return BAD;
			//*bp == ' '
			++bp;
			state(READ_PORT);
		case READ_PORT:
			idbgx(Dbg::any, "READ_PORT");
			bb = bp;
			while(bp != be){
				if(*bp == '\n') break;
				++bp;
			}
			port.append(bb, bp - bb);
			if(bp == be) return doRefill();
			if(port.size() > 64) return BAD;
			if(port.size() && port[port.size() - 1] == '\r'){
				port.resize(port.size() - 1);
			}
			//*bp == '\n'
			++bp;
			state(REGISTER_CONNECTION);
			return OK;
	}
	cassert(false);
	return BAD;
}
int MultiConnection::doProxy(const TimeSpec &_tout){
	int retv = NOK;
	if((socketEvents(0) & fdt::ERRDONE) || (socketEvents(1) & fdt::ERRDONE)){
		idbg("bad errdone "<<socketEvents(1)<<' '<<fdt::ERRDONE);
		return BAD;
	}
	switch(socketState(0)){
		case Receive:
			idbg("receive 0");
			switch(socketRecv(0, stubs[0].recvbuf.data, Buffer::Capacity)){
				case BAD:
					idbg("bad recv 0");
					return BAD;
				case OK:
					idbg("receive ok 0");
					socketState(0, Send);
					retv = OK;
					break;
				case NOK:
					idbg("receive nok 0");
					socketTimeoutRecv(0, 30);
					socketState(0, ReceiveWait);
					break;
			}
			break;
		case ReceiveWait:
			idbg("receivewait 0");
			if(socketEvents(0) & fdt::INDONE){
				socketState(0, Send);
			}else break;
		case Send:
			idbg("send 0");
			switch(socketSend(1, stubs[0].recvbuf.data, socketRecvSize(0))){
				idbg("bad send 0");
				case BAD:
					return BAD;
				case OK:
					idbg("send ok 0");
					socketState(0, Receive);
					retv = OK;
					break;
				case NOK:
					idbg("send nok 0");
					socketState(0, SendWait);
					socketTimeoutSend(1, 50);
					break;
			}
			break;
		case SendWait:
			idbg("sendwait 0");
			if(socketEvents(1) & fdt::OUTDONE){
				socketState(0, Receive);
			}
			break;
	}
	
	switch(socketState(1)){
		idbg("receive 1");
		case Receive:
			switch(socketRecv(1, stubs[1].recvbuf.data, Buffer::Capacity)){
				case BAD:
					idbg("bad recv 1");
					return BAD;
				case OK:
					idbg("receive ok 1");
					socketState(1, Send);
					retv = OK;
					break;
				case NOK:
					idbg("receive nok 1");
					socketTimeoutRecv(1, 50);
					socketState(1, ReceiveWait);
					break;
			}
			break;
		case ReceiveWait:
			idbg("receivewait 1");
			if(socketEvents(1) & fdt::INDONE){
				socketState(1, Send);
			}else break;
		case Send:
			idbg("send 1");
			switch(socketSend(0, stubs[1].recvbuf.data, socketRecvSize(1))){
				case BAD:
					idbg("bad recv 1");
					return BAD;
				case OK:
					idbg("send ok 1");
					socketState(1, Receive);
					retv = OK;
					break;
				case NOK:
					idbg("send nok 1");
					socketState(1, SendWait);
					socketTimeoutSend(0, 30);
					break;
			}
			break;
		case SendWait:
			idbg("sendwait 1");
			if(socketEvents(0) & fdt::OUTDONE){
				socketState(1, Receive);
				retv = OK;
			}
			break;
	}
	idbg("retv "<<retv);
	return retv;
}

int MultiConnection::doRefill(){
	idbgx(Dbg::any, "");
	if(bp == NULL){//we need to issue a read
		switch(socketRecv(0, stubs[0].recvbuf.data, Buffer::Capacity)){
			case BAD:	return BAD;
			case OK:
				bp = stubs[0].recvbuf.data;
				be = stubs[0].recvbuf.data + socketRecvSize(0);
				return OK;
			case NOK:
				be = NULL;
				bp = stubs[0].recvbuf.data;
				idbgx(Dbg::any, "NOK");
				return NOK;
		}
	}
	if(be == NULL){
		if(socketEvents(0) & fdt::INDONE){
			be = stubs[0].recvbuf.data + socketRecvSize(0);
		}else{
			idbgx(Dbg::any, "NOK");
			return NOK;
		}
	}
	return OK;
}

}//namespace proxy
}//namespace concept

