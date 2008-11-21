/* Implementation file connection.hpp
	
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
#ifndef AIO_CONNECTION_HPP
#define AIO_CONNECTION_HPP

#include "clientserver/aio/aioobject.hpp"

class SocketDevice;
class SocketAddress;
class AddrInfoIterator;

namespace clientserver{

namespace aio{

namespace tcp{

class Connection: public Object{
public:
	Connection(Socket *_psock = NULL);
	Connection(const SocketDevice &_rsd);
	~Connection();
	bool socketOk()const;
	int socketConnect(const AddrInfoIterator&);
	bool socketIsSecure()const;
	int socketSend(const char* _pb, uint32 _bl, uint32 _flags = 0);
	int socketRecv(char *_pb, uint32 _bl, uint32 _flags = 0);
	uint32 socketRecvSize()const;
	const uint64& socketSendCount()const;
	const uint64& socketRecvCount()const;
	bool socketHasPendingSend()const;
	bool socketHasPendingRecv()const;
	int socketLocalAddress(SocketAddress &_rsa)const;
	int socketRemoteAddress(SocketAddress &_rsa)const;
	void socketTimeout(const TimeSpec &_crttime, ulong _addsec, ulong _addnsec = 0);
	uint32 socketEvents()const;
	void socketErase();
	int socketSet(Socket *_psock);
	int socketSet(const SocketDevice &_rsd);
	int socketCreate4();
	int socketCreate6();
	int socketCreate(const AddrInfoIterator&);
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

