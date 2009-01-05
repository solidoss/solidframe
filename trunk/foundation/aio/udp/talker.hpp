/* Implementation file talker.hpp
	
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
#ifndef AIO_TALKER_HPP
#define AIO_TALKER_HPP

#include "foundation/aio/aioobject.hpp"

class SocketDevice;
class SocketAddress;
class SockAddrPair;
class AddrInfoIterator;

namespace foundation{

namespace aio{

namespace udp{

class Talker: public Object{
public:
	Talker(Socket *_psock = NULL);
	Talker(const SocketDevice &_rsd);
	~Talker();
	bool socketOk()const;
	int socketSend(const char *_pb, uint32 _bl, const SockAddrPair &_sap, uint32 _flags = 0);
	int socketRecv(char *_pb, uint32 _bl, uint32 _flags = 0);
	uint32 socketRecvSize()const;
	const SockAddrPair &socketRecvAddr() const;
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
	int socketCreate4(const SockAddrPair &_sap);
	int socketCreate6(const SockAddrPair &_sap);
	int socketCreate(const AddrInfoIterator&);
	void socketRequestRegister();
	void socketRequestUnregister();
	
	int socketState()const;
	void socketState(int _st);
	
private:
	SocketStub	stub;
	int32		req;
	int32		res;
	int32		tout;
};

}//namespace udp

}//namespace aio

}//namespace foundation


#endif

