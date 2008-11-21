/* Implementation file betaconnection.cpp
	
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

#include "core/server.hpp"
#include "beta/betaservice.hpp"
#include "betaconnection.hpp"
#include "system/socketaddress.hpp"
#include "system/debug.hpp"
#include "system/timespec.hpp"
#include "system/cassert.hpp"

namespace cs=clientserver;
static char	*hellostr = "Welcome to echo service!!!\r\n"; 

namespace test{

namespace beta{
Connection::Connection(const char *_node, const char *_srv): 
									bend(bbeg + BUFSZ),brpos(bbeg),bwpos(bbeg),
									pai(NULL){
	cassert(_node && _srv);
	pai = new AddrInfo(_node, _srv, 0, AddrInfo::Inet4, AddrInfo::Stream);
	it = pai->begin();
	state(CONNECT);
	
}
Connection::Connection(const SocketDevice &_rsd):
	BaseTp(_rsd),
	bend(bbeg + BUFSZ),brpos(bbeg),bwpos(bbeg),
	pai(NULL)
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
	test::Server &rs = test::Server::the();
	rs.service(*this).removeConnection(*this);
	delete pai;
}

int Connection::execute(ulong _sig, TimeSpec &_tout){
	_tout.add(20);//allways set it if it's not MAXTIMEOUT
	if(_sig & (cs::TIMEOUT | cs::ERRDONE)){
		idbg("connecton timeout or error");
		if(state() == CONNECT_TOUT){
			cassert(!socketHasPendingSend());
			//idbg("are pending sends = "<<channel().arePendingSends());
			if(++it){
				state(CONNECT);
				return cs::UNREGISTER;
			}
		}
		return BAD;
	}
	if(signaled()){
		test::Server &rs = test::Server::the();
		{
		Mutex::Locker	lock(rs.mutex(*this));
		ulong sm = grabSignalMask();
		if(sm & cs::S_KILL) return BAD;
		}
	}
	int rc = 512 * 1024;
	do{
		switch(state()){
			case READ:
				switch(socketRecv(bbeg, BUFSZ)){
					case BAD: return BAD;
					case OK: break;
					case NOK: state(READ_TOUT); return NOK;
				}
			case READ_TOUT:
				state(WRITE);
			case WRITE:
				switch(socketSend(bbeg, socketRecvSize())){
					case BAD: return BAD;
					case OK: break;
					case NOK: state(WRITE_TOUT); return NOK;
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
					case NOK: state(CONNECT_TOUT); return cs::REGISTER;
				};
				break;
			case CONNECT_TOUT:
				delete pai; pai = NULL;
			case INIT:{
				char			host[SocketAddress::MaxSockHostSz];
				char			port[SocketAddress::MaxSockServSz];
				SocketAddress	addr;
				socketRemoteAddress(addr);
				addr.name(
					host,
					SocketAddress::MaxSockHostSz,
					port,
					SocketAddress::MaxSockServSz,
					SocketAddress::NumericService
				);
				idbg("remote host = "<<host<<" remote port = "<<port);
				socketSend(hellostr, strlen(hellostr));
				state(READ);
				}break;
		}
		rc -= socketRecvSize();
	}while(rc > 0);
	return OK;
}

int Connection::execute(){
	return BAD;
}

int Connection::accept(cs::Visitor &_rov){
	//static_cast<TestInspector&>(_roi).inspectConnection(*this);
	return -1;
}
}//namespace echo
}//namespace test

