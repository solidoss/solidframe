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

//! A class for single socket asynchronous UDP communication
/*!
	Inherit this class if you need single socket UDP communication.
	If your protocol need two or more socket to pass data to eachother
	use MultiTalker instead.<br>
	All methods marked as asynchronous have three return values:<br>
	- BAD (-1) for unrecoverable error like socket connection closed;<br>
	- NOK (1) opperation is pending for completion within aio::Selector;<br>
	- OK (0) opperation completed successfully.<br>
*/
class Talker: public Object{
public:
	//! Constructor using an aio::Socket
	Talker(Socket *_psock = NULL);
	//! Constructor using a SocketDevice
	Talker(const SocketDevice &_rsd);
	//! Destructor
	~Talker();
	//! Returs true if the socket is ok
	bool socketOk()const;
	//! Asynchronous send
	int socketSend(const char *_pb, uint32 _bl, const SockAddrPair &_sap, uint32 _flags = 0);
	//! Asynchronous receive
	int socketRecv(char *_pb, uint32 _bl, uint32 _flags = 0);
	//! Get the size of the received data
	/*!
		Call this on successful completion of socketRecv
	*/
	uint32 socketRecvSize()const;
	//! Gets the peer (sender) address of the received data
	/*!
		Call this on successful completion of socketRecv
	*/
	const SockAddrPair &socketRecvAddr() const;
	//! The ammount of data sent
	const uint64& socketSendCount()const;
	//! The ammount of data received
	const uint64& socketRecvCount()const;
	//! Return true if the socket is already waiting for a send completion
	bool socketHasPendingSend()const;
	//! Return true if the socket is already waiting for a receive completion
	bool socketHasPendingRecv()const;
	//! Get the local address of the socket
	int socketLocalAddress(SocketAddress &_rsa)const;
	//! Get the remote address of the socket
	int socketRemoteAddress(SocketAddress &_rsa)const;
	//! Set the timeout for asynchronous opperation completion
	void socketTimeout(const TimeSpec &_crttime, ulong _addsec, ulong _addnsec = 0);
	//! Gets the mask with completion events for the socket.
	uint32 socketEvents()const;
	//! Erase the socket - call socketRequestRegister before
	void socketErase();
	//! Sets the socket given an aio::Socket
	int socketSet(Socket *_psock);
	//! Sets the socket given a SocketDevice
	int socketSet(const SocketDevice &_rsd);
	//! Create a socket for ipv4 - use connect after
	int socketCreate4(const SockAddrPair &_sap);
	//! Create a socket for ipv6 - use connect after
	int socketCreate6(const SockAddrPair &_sap);
	//! Create a socket given an address
	int socketCreate(const AddrInfoIterator&);
	//! Request for registering the socket onto the aio::Selector
	void socketRequestRegister();
	//! Request for unregistering the socket onto the aio::Selector
	/*!
		The sockets are automatically unregistered and erased
		on object destruction.
	*/
	void socketRequestUnregister();
	//! Gets the state associated to the socket
	int socketState()const;
	//! Sets the state associated to the socket
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

