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

#include "foundation/aio/aioobject.hpp"

class SocketDevice;
class SocketAddress;
class AddrInfoIterator;

namespace foundation{

namespace aio{

class SecureSocket;

namespace tcp{
//! A simple Connection object optimized for single channel tcp communication
/*!
	Lots of protocols (like IMAP, SMTP, POP etc) use single separate connections.
	The aio::tcp::Connection should be used for implementing this kind of protocols.
	It avoids an extra allocation used in case of MultiConnections.
*/
class Connection: public Object{
public:
	//! Constructor using an aio::Socket
	/*! Constructor using an aio::Socket
		The aio::Socket is a forward declaration to a private class
		of aio library.
	*/
	Connection(Socket *_psock = NULL);
	//! Constructor using a SocketDevice
	Connection(const SocketDevice &_rsd);
	~Connection();
	//! Returs true if the socket is ok
	bool socketOk()const;
	//! Initiate an asynchronous connect
	/*!
		\param _rai An AddrInfo iterator holding the destination address.
		\retval _ OK when opperation completed right away, BAD when something 
		bad happened, NOK when needing to wait for completion.
	*/
	int socketConnect(const AddrInfoIterator& _rai);
	//! Returns true if the socket is secure - SSL
	bool socketIsSecure()const;
	//! Initiate an asynchronous send
	/*!
		\retval _ OK when opperation completed right away, BAD when something 
		bad happened, NOK when needing to wait for completion.
	*/
	int socketSend(const char* _pb, uint32 _bl, uint32 _flags = 0);
	//! Initiate an asynchronous receive
	/*!
		\retval _ OK when opperation completed right away, BAD when something 
		bad happened, NOK when needing to wait for completion.
	*/
	int socketRecv(char *_pb, uint32 _bl, uint32 _flags = 0);
	//! Get the size of the received data
	/*!
		Call this on successful completion of socketRecv
	*/
	uint32 socketRecvSize()const;
	//! The ammount of data sent
	const uint64& socketSendCount()const;
	//! The ammount of data received
	const uint64& socketRecvCount()const;
	//! Return true if it already waiting for a receive completion
	bool socketHasPendingSend()const;
	//! Return true if it already waiting for a send completion
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
	int socketCreate4();
	//! Create a socket for ipv6 - use connect after
	int socketCreate6();
	//! Create a socket given a address
	int socketCreate(const AddrInfoIterator&);
	//! Request for registering the socket to the aio::Selector
	void socketRequestRegister();
	//! Request for registering the socket to the aio::Selector
	/*!
		The sockets are automatically unregistered and erased
		on object destruction.
	*/
	void socketRequestUnregister();
	
	int socketState()const;
	void socketState(int _st);
	
	SecureSocket* socketSecureSocket();
	void socketSecureSocket(SecureSocket *_pss);
	
	int socketSecureAccept();
	int socketSecureConnect();
private:
	SocketStub	stub;
	int32		req;
	int32		res;
	int32		tout;
};

}//namespace tcp

}//namespace aio

}//namespace foundation


#endif

