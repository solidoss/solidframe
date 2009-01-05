/* Implementation file echotalker.cpp
	
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

#include <cstdio>

#include "core/manager.hpp"
#include "echo/echoservice.hpp"
#include "echotalker.hpp"
#include "system/timespec.hpp"
#include "system/socketaddress.hpp"
#include "system/debug.hpp"
#include "system/mutex.hpp"

namespace cs = foundation;

static const char * const hellostr = "Hello from echo udp client talker!!!\r\n"; 

namespace test{

namespace echo{

Talker::Talker(const char *_node, const char *_srv):pai(NULL){
	if(_node){
		pai = new AddrInfo(_node, _srv);
		strcpy(bbeg, hellostr);
		sz = strlen(hellostr);
		state(INIT);
	}
}

Talker::Talker(const SocketDevice &_rsd):BaseTp(_rsd), pai(NULL){
	state(READ);
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

Talker::~Talker(){
	test::Manager &rm = test::Manager::the();
	rm.service(*this).removeTalker(*this);
	delete pai;
}
int Talker::execute(ulong _sig, TimeSpec &_tout){
	if(_sig & (cs::TIMEOUT | cs::ERRDONE)){
		if(_sig & cs::TIMEOUT)
			idbg("talker timeout");
		if(_sig & cs::ERRDONE)
			idbg("talker error");
		if(state() != WRITE && state() != WRITE2)	return BAD;
	}
	if(signaled()){
		test::Manager &rm = test::Manager::the();
		{
		Mutex::Locker	lock(rm.mutex(*this));
		ulong sm = grabSignalMask(0);
		if(sm & cs::S_KILL) return BAD;
		}
	}
	if(socketEvents() & cs::ERRDONE) return BAD;
	int rc = 512 * 1024;
	do{
		switch(state()){
			case READ:
				switch(socketRecv(bbeg, BUFSZ)){
					case BAD: return BAD;
					case OK: state(READ_TOUT);break;
					case NOK:
						socketTimeout(_tout, 20);
						state(READ_TOUT); 
						return NOK;
				}
			case READ_TOUT:
				state(WRITE);
			case WRITE:
				sprintf(bbeg + socketRecvSize() - 1," [%u:%d]\r\n", (unsigned)_tout.seconds(), (int)_tout.nanoSeconds());
				switch(socketSend(bbeg, strlen(bbeg), socketRecvAddr())){
					case BAD: return BAD;
					case OK: break;
					case NOK: state(WRITE_TOUT);
						socketTimeout(_tout, 20);
						return NOK;
				}
			case WRITE_TOUT:
				state(WRITE2);
				socketTimeout(_tout, 20);
				return NOK;
			case WRITE2:
				sprintf(bbeg + socketRecvSize() - 1," [%u:%d]\r\n", (unsigned)_tout.seconds(), (int)_tout.nanoSeconds());
				switch(socketSend(bbeg, strlen(bbeg), socketRecvAddr())){
					case BAD: return BAD;
					case OK: break;
					case NOK: state(WRITE_TOUT2);
						socketTimeout(_tout, 20);
						return NOK;
				}
			case WRITE_TOUT2:
				state(WRITE_TOUT);
				break;
			case INIT:
				if(pai->empty()){
					idbg("Invalid address");
					return BAD;
				}
				AddrInfoIterator it(pai->begin());
				switch(socketSend(bbeg, sz, SockAddrPair(it))){
					case BAD: return BAD;
					case OK: state(READ); break;
					case NOK:
						state(WRITE_TOUT);
						return NOK;
				}
				break;
		}
		rc -= socketRecvSize();
	}while(rc > 0);
	return OK;
}

int Talker::execute(){
	return BAD;
}


int Talker::accept(cs::Visitor &_rov){
	//static_cast<TestInspector&>(_roi).inspectTalker(*this);
	return -1;
}
}//namespace echo
}//namespace test

