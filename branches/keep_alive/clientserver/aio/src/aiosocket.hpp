/* Declarations file aiosocket.hpp
	
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

#ifndef AIO_SOCKET_HPP
#define AIO_SOCKET_HPP

#include "system/socketdevice.hpp"
#include "system/common.hpp"

struct SocketAddress;
struct SockAddrPair;

namespace clientserver{

namespace aio{

class Selector;

class Socket{
protected:
	enum Type{
		ACCEPTOR,
		CHANNEL,
		STATION,
	};
	Socket(Type _tp);
		
	bool ok()const;
	//! Create the socket
	int create(const AddrInfoIterator& _rai);
	//! Asynchronous connect request
	int connect(const AddrInfoIterator& _rai);
	int accept(Socket &_rs);
	//! Send a buffer
	int send(const char* _pb, uint32 _bl, uint32 _flags = 0);
	//! Receives data into a buffer
	int recv(char *_pb, uint32 _bl, uint32 _flags = 0);
	//! The size of the buffer received
	uint32 recvSize()const;
	//! The amount of data sent
	const uint64& sendCount()const;
	//! The amount of data received.
	const uint64& recvCount()const;
	//! Return true if there are pending send opperations
	bool isSendPending()const;
	//! Return true if there are pending recv opperations
	bool isRecvPending()const;
	//! Return the local address of the socket
	int localAddress(SocketAddress &_rsa)const;
	//! Return the remote address of the socket
	int remoteAddress(SocketAddress &_rsa)const;
	
	//! Asynchrounous recv_from  call
	int recvFrom(char *_pb, uint32 _bl, uint32 _flags = 0);
	//! Asynchrounous send_to call
	int sendTo(const char *_pb, uint32 _bl, const SockAddrPair &_sap, uint32 _flags = 0);
	//! The sender address for last received data.
	const SockAddrPair &recvAddr() const;
private:
	friend class Selector;
	void doPrepare();
	void doUnprepare();
	void doClear();
	
	int doSend();
	int doRecv();
	
	uint32 ioRequest()const;
	int descriptor()const{return sd.descriptor();}
private:
	struct StationData;
	struct AcceptorData;
	SocketDevice	sd;
	Type			type;
	uint64			rcvcnt;
	uint64			sndcnt;
	char*			rcvbuf;
	const char*		sndbuf;
	uint32			rcvlen;
	uint32			sndlen;
	union{
		StationData		*psd;
		AcceptorData	*pad;
	}d;
};


}//namespace aio


}//namespace clientserver


#endif

