/* Implementation file listener.cpp
	
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

#include "foundation/aio/tcp/listener.hpp"
#include "foundation/aio/src/aiosocket.hpp"
#include "system/cassert.hpp"

namespace foundation{

namespace aio{

namespace tcp{

Listener::Listener(Socket *_psock):
	Object(&stub, 1, &req, &res, &tout), req(-1), res(-1), tout(-1)
{
	stub.psock = _psock;
}

Listener::Listener(const SocketDevice &_rsd):
	Object(&stub, 1, &req, &res, &tout), req(-1), res(-1), tout(-1)
{
	if(_rsd.ok()){
		stub.psock = new Socket(Socket::ACCEPTOR, _rsd);
	}
}

bool Listener::socketOk()const{
	return stub.psock->ok();
}

int Listener::socketAccept(SocketDevice &_rsd){
	int rv = stub.psock->accept(_rsd);
	if(rv == NOK){
		pushRequest(0, SocketStub::IORequest);
	}
	return rv;
}

void Listener::socketErase(){
	delete stub.psock;
	stub.psock = NULL;
}
uint Listener::socketSet(Socket *_psock){
	cassert(!stub.psock);
	stub.psock = _psock;
	return 0;
}
uint Listener::socketSet(const SocketDevice &_rsd){
	cassert(!stub.psock);
	if(_rsd.ok()){
		stub.psock = new Socket(Socket::ACCEPTOR, _rsd);
	}
	return 0;
}
void Listener::socketRequestRegister(){
	//ensure that we dont have double request
	cassert(stub.request <= SocketStub::Response);
	stub.request = SocketStub::RegisterRequest;
	*reqpos = 0;
	++reqpos;
}
void Listener::socketRequestUnregister(){
	//ensure that we dont have double request
	cassert(stub.request <= SocketStub::Response);
	stub.request = SocketStub::UnregisterRequest;
	*reqpos = 0;
	++reqpos;
}

int Listener::socketState()const{
	return stub.state;
}
void Listener::socketState(int _st){
	stub.state = _st;
}

}//namespace tcp

}//namespace aio

}//namespace foundation
