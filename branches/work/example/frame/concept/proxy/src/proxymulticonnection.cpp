// proxymulticonnection.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "core/manager.hpp"
#include "proxy/proxyservice.hpp"
#include "proxymulticonnection.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/timespec.hpp"

using namespace solid;
//static const char	*hellostr = "Welcome to proxy service!!!\r\n"; 

namespace concept{

namespace proxy{

MultiConnection::MultiConnection(
	const char *_node,
	const char *_srv
): b(false){
	cassert(false);
}
MultiConnection::MultiConnection(const SocketDevice &_rsd):
	BaseT(_rsd), b(false)
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
}
enum{
		Receive,
		ReceiveWait,
		Send,
		SendWait
	};
/*virtual*/ void MultiConnection::execute(ExecuteContext &_rexectx){
	idbg("time.sec "<<_rexectx.currentTime().seconds()<<" time.nsec = "<<_rexectx.currentTime().nanoSeconds());
	if(_rexectx.eventMask() & (frame::EventTimeout | frame::EventDoneError | frame::EventTimeoutRecv | frame::EventTimeoutSend)){
		idbg("connecton timeout or error : tout "<<frame::EventTimeout<<" err "<<frame::EventDoneError<<" toutrecv "<<frame::EventTimeoutRecv<<" tout send "<<frame::EventTimeoutSend);
		//We really need this check as epoll upsets if we register an unconnected socket
		if(state() != CONNECT){
			//lets see which socket has timeout:
			while(this->signaledSize()){
				solid::uint evs = socketEvents(signaledFront());
				idbg("for "<<signaledFront()<<" evs "<<evs);
				this->signaledPop();
			}
			_rexectx.close();
			return;
		}
	}
	if(notified()){
		concept::Manager &rm = concept::Manager::the();
		Locker<Mutex>	lock(rm.mutex(*this));
		ulong sm = grabSignalMask();
		if(sm & frame::S_KILL){
			_rexectx.close();
			return;
		}
	}
	AsyncE rv = AsyncWait;
	switch(state()){
		case READ_ADDR:
			idbgx(Debug::any, "READ_ADDR");
		case READ_PORT:
			idbgx(Debug::any, "READ_PORT");
			rv = doReadAddress();
			break;
		case REGISTER_CONNECTION:{
			idbgx(Debug::any, "REGISTER_CONNECTION");
			rd = synchronous_resolve(addr.c_str(), port.c_str());
			it = rd.begin();
			SocketDevice	sd;
			sd.create(it);
			sd.makeNonBlocking();
			socketRequestRegister(socketInsert(sd));
			state(CONNECT);
			}return;
		case CONNECT://connect the other end:
			//TODO: check if anything went wrong
			while(this->signaledSize()){
				this->signaledPop();
			}
			idbgx(Debug::any, "CONNECT");
			switch(socketConnect(1, it)){
				case frame::aio::AsyncError:
					idbgx(Debug::any, "Failure");
					_rexectx.close();
					return;
				case frame::aio::AsyncSuccess:
					state(SEND_REMAINS);
					idbgx(Debug::any, "Success");
					_rexectx.reschedule();
					return;
				case frame::aio::AsyncWait:
					state(CONNECT_TOUT);
					idbgx(Debug::any, "Wait");
					return;
			};
		case CONNECT_TOUT:{
			idbgx(Debug::any, "CONNECT_TOUT");
			uint32 evs = socketEvents(1);
			socketState(0, Receive);
			socketState(1, Receive);
			if(!evs || !(evs & frame::EventDoneSend)){
				_rexectx.close();
				return;
			}
			state(SEND_REMAINS);
			}
		case SEND_REMAINS:{
			idbgx(Debug::any, "SEND_REMAINS");
			state(PROXY);
			if(bp != be){
				switch(socketSend(1, bp, be - bp)){
					case frame::aio::AsyncError:
						idbgx(Debug::any, "Failure");
						_rexectx.close();
						return;
					case frame::aio::AsyncSuccess:
						idbgx(Debug::any, "Success");
						break;
					case frame::aio::AsyncWait:
						stubs[0].recvbuf.usecnt = 1;
						idbgx(Debug::any, "Wait");
						return;
				}
			}
			}
		case PROXY:
			idbgx(Debug::any, "PROXY");
			rv =  doProxy(_rexectx.currentTime());
	}
	while(this->signaledSize()){
		this->signaledPop();
	}
	if(rv == solid::AsyncError){
		_rexectx.close();
	}else if(rv == solid::AsyncSuccess){
		_rexectx.reschedule();
	}
}
AsyncE MultiConnection::doReadAddress(){
	if(bp and !be) return doRefill();
	char *bb = bp;
	switch(state()){
		case READ_ADDR:
			idbgx(Debug::any, "READ_ADDR");
			while(bp != be){
				if(*bp == ' ') break;
				++bp;
			}
			addr.append(bb, bp - bb);
			if(bp == be) return doRefill();
			if(addr.size() > 64) return solid::AsyncError;
			//*bp == ' '
			++bp;
			state(READ_PORT);
		case READ_PORT:
			idbgx(Debug::any, "READ_PORT");
			bb = bp;
			while(bp != be){
				if(*bp == '\n') break;
				++bp;
			}
			port.append(bb, bp - bb);
			if(bp == be) return doRefill();
			if(port.size() > 64) return solid::AsyncError;
			if(port.size() && port[port.size() - 1] == '\r'){
				port.resize(port.size() - 1);
			}
			//*bp == '\n'
			++bp;
			state(REGISTER_CONNECTION);
			return solid::AsyncSuccess;
	}
	cassert(false);
	return solid::AsyncError;
}
AsyncE MultiConnection::doProxy(const TimeSpec &_tout){
	AsyncE retv = solid::AsyncWait;
	if((socketEvents(0) & frame::EventDoneError) || (socketEvents(1) & frame::EventDoneError)){
		idbg("bad errdone "<<socketEvents(1)<<' '<<frame::EventDoneError);
		return solid::AsyncError;
	}
	switch(socketState(0)){
		case Receive:
			idbg("receive 0");
			switch(socketRecv(0, stubs[0].recvbuf.data, Buffer::Capacity)){
				case frame::aio::AsyncError:
					idbg("bad recv 0");
					return solid::AsyncError;
				case frame::aio::AsyncSuccess:
					idbg("receive ok 0");
					socketState(0, Send);
					retv = solid::AsyncSuccess;
					break;
				case frame::aio::AsyncWait:
					idbg("receive nok 0");
					socketTimeoutRecv(0, 30);
					socketState(0, ReceiveWait);
					break;
			}
			break;
		case ReceiveWait:
			idbg("receivewait 0");
			if(socketEvents(0) & frame::EventDoneRecv){
				socketState(0, Send);
			}else break;
		case Send:
			idbg("send 0");
			switch(socketSend(1, stubs[0].recvbuf.data, socketRecvSize(0))){
				case frame::aio::AsyncError:
					return solid::AsyncError;
				case frame::aio::AsyncSuccess:
					idbg("send ok 0");
					socketState(0, Receive);
					retv = solid::AsyncSuccess;
					break;
				case frame::aio::AsyncWait:
					idbg("send nok 0");
					socketState(0, SendWait);
					socketTimeoutSend(1, 50);
					break;
			}
			break;
		case SendWait:
			idbg("sendwait 0");
			if(socketEvents(1) & frame::EventDoneSend){
				socketState(0, Receive);
			}
			break;
	}
	
	switch(socketState(1)){
		idbg("receive 1");
		case Receive:
			switch(socketRecv(1, stubs[1].recvbuf.data, Buffer::Capacity)){
				case frame::aio::AsyncError:
					idbg("bad recv 1");
					return solid::AsyncError;
				case frame::aio::AsyncSuccess:
					idbg("receive ok 1");
					socketState(1, Send);
					retv = solid::AsyncSuccess;
					break;
				case frame::aio::AsyncWait:
					idbg("receive nok 1");
					socketTimeoutRecv(1, 50);
					socketState(1, ReceiveWait);
					break;
			}
			break;
		case ReceiveWait:
			idbg("receivewait 1");
			if(socketEvents(1) & frame::EventDoneRecv){
				socketState(1, Send);
			}else break;
		case Send:
			idbg("send 1");
			switch(socketSend(0, stubs[1].recvbuf.data, socketRecvSize(1))){
				case frame::aio::AsyncError:
					idbg("bad recv 1");
					return solid::AsyncError;
				case frame::aio::AsyncSuccess:
					idbg("send ok 1");
					socketState(1, Receive);
					retv = solid::AsyncSuccess;
					break;
				case frame::aio::AsyncWait:
					idbg("send nok 1");
					socketState(1, SendWait);
					socketTimeoutSend(0, 30);
					break;
			}
			break;
		case SendWait:
			idbg("sendwait 1");
			if(socketEvents(0) & frame::EventDoneSend){
				socketState(1, Receive);
				retv = solid::AsyncSuccess;
			}
			break;
	}
	idbg("retv "<<retv);
	return retv;
}

AsyncE MultiConnection::doRefill(){
	idbgx(Debug::any, "");
	if(bp == NULL){//we need to issue a read
		switch(socketRecv(0, stubs[0].recvbuf.data, Buffer::Capacity)){
			case frame::aio::AsyncError:	return solid::AsyncError;
			case frame::aio::AsyncSuccess:
				bp = stubs[0].recvbuf.data;
				be = stubs[0].recvbuf.data + socketRecvSize(0);
				return solid::AsyncSuccess;
			case frame::aio::AsyncWait:
				be = NULL;
				bp = stubs[0].recvbuf.data;
				idbgx(Debug::any, "NOK");
				return solid::AsyncWait;
		}
	}
	if(be == NULL){
		if(socketEvents(0) & frame::EventDoneRecv){
			be = stubs[0].recvbuf.data + socketRecvSize(0);
		}else{
			idbgx(Debug::any, "Wait");
			return solid::AsyncWait;
		}
	}
	return solid::AsyncSuccess;
}

}//namespace proxy
}//namespace concept

