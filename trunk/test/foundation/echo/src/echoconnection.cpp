/* Implementation file echoconnection.cpp
	
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

#include "core/manager.hpp"
#include "echo/echoservice.hpp"
#include "echoconnection.hpp"
#include "system/socketaddress.hpp"
#include "system/debug.hpp"
#include "system/timespec.hpp"
#include "system/cassert.hpp"

namespace fdt=foundation;
static char	*hellostr = "Welcome to echo service!!!\r\n"; 

namespace test{

namespace echo{
Connection::Connection(const char *_node, const char *_srv): 
	BaseTp(NULL),
	bend(bbeg + BUFSZ),brpos(bbeg),bwpos(bbeg),
	pai(NULL),b(false)
{
	cassert(_node && _srv);
	pai = new AddrInfo(_node, _srv);
	it = pai->begin();
	state(CONNECT);
	
}
Connection::Connection(const SocketDevice &_rsd):
	BaseTp(_rsd),bend(bbeg + BUFSZ),brpos(bbeg),bwpos(bbeg),
	pai(NULL),b(false)
{
	state(INIT);
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

Connection::~Connection(){
	test::Manager &rm = test::Manager::the();
	rm.service(*this).removeConnection(*this);
	delete pai;
}

int Connection::execute(ulong _sig, TimeSpec &_tout){
	idbg("time.sec "<<_tout.seconds()<<" time.nsec = "<<_tout.nanoSeconds());
	if(_sig & (fdt::TIMEOUT | fdt::ERRDONE)){
		idbg("connecton timeout or error");
		if(state() == CONNECT_TOUT){
			if(++it){
				state(CONNECT);
				return fdt::UNREGISTER;
			}
		}
		return BAD;
	}

	if(signaled()){
		test::Manager &rs = test::Manager::the();
		{
		Mutex::Locker	lock(rs.mutex(*this));
		ulong sm = grabSignalMask();
		if(sm & fdt::S_KILL) return BAD;
		}
	}
	if(socketEvents()){
		if(socketEvents() == fdt::ERRDONE){
			
			return BAD;
		}
		if(state() == READ_TOUT){	
			cassert(socketEvents() & fdt::INDONE);
		}else if(state() == WRITE_TOUT){	
			cassert(socketEvents() & fdt::OUTDONE);
		}
		
	}
	int rc = 512 * 1024;
	do{
		switch(state()){
			case READ:
				switch(socketRecv(bbeg, BUFSZ)){
					case BAD: return BAD;
					case OK: break;
					case NOK: 
						state(READ_TOUT); b=true; 
						socketTimeout(_tout, 30);
						return NOK;
				}
			case READ_TOUT:
				state(WRITE);
			case WRITE:
				switch(socketSend(bbeg, socketRecvSize())){
					case BAD: return BAD;
					case OK: break;
					case NOK: 
						state(WRITE_TOUT);
						socketTimeout(_tout, 10);
						return NOK;
				}
			case WRITE_TOUT:
				state(READ);
				break;
			case CONNECT:
				switch(socketConnect(it)){
					case BAD:
						if(++it){
							state(CONNECT);
							return OK;
						}
						return BAD;
					case OK:  state(INIT);break;
					case NOK: state(CONNECT_TOUT); return fdt::REGISTER;
				};
				break;
			case CONNECT_TOUT:
				delete pai; pai = NULL;
			case INIT:
				socketSend(hellostr, strlen(hellostr));
				state(READ);
				break;
		}
		rc -= socketRecvSize();
	}while(rc > 0);
	return OK;
}

int Connection::execute(){
	return BAD;
}

int Connection::accept(fdt::Visitor &_rov){
	//static_cast<TestInspector&>(_roi).inspectConnection(*this);
	return -1;
}
}//namespace echo
}//namespace test

