/* Implementation file listener.hpp
	
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
#ifndef AIO_LISTENER_HPP
#define AIO_LISTENER_HPP

#include "clientserver/aio/aioobject.hpp"

class SocketDevice;
class SocketAddress;
class AddrInfoIterator;

namespace clientserver{

namespace aio{

namespace tcp{

class Listener: public Object{
public:
	Listener(Socket *_psock = NULL);
	Listener(const SocketDevice &_rsd);
	
	bool socketOk()const;
	
	int socketAccept(SocketDevice &_rsd);
	
	void socketErase();
	uint socketSet(Socket *_psock);
	uint socketSet(const SocketDevice &_rsd);
	void socketRequestRegister();
	void socketRequestUnregister();
	
	int state()const;
	void state(int _st);
private:
	SocketStub	stub;
	int32		req;
	int32		res;
	int32		tout;
};

}//namespace tcp

}//namespace aio

}//namespace clientserver


#endif
