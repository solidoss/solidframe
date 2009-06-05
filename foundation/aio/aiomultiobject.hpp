/* Implementation file aiomultiobject.hpp
	
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

#ifndef AIO_MULTI_OBJECT_HPP
#define AIO_MULTI_OBJECT_HPP

#include "foundation/aio/aioobject.hpp"
#include "utility/stack.hpp"

class SocketDevice;
class SocketAddress;
class SockAddrPair;
class AddrInfoIterator;


namespace foundation{

namespace aio{

class SecureSocket;

//! A class for multi socket asynchronous communication
/*!
	Inherit this class if your protocol needs two or more
	connections to pass data to eachother (e.g. proxies, 
	chat-rooms etc.)<br>
	- BAD (-1) for unrecoverable error like socket connection closed;<br>
	- NOK (1) opperation is pending for completion within aio::Selector;<br>
	- OK (0) opperation completed successfully.<br>
*/

class MultiObject: public Object{
public:
	//! Constructor given an aio::Socket (it will be inserted on position zero)
	MultiObject(Socket *_psock = NULL);
	//! Constructor given a SocketDevice (it will be inserted on position zero)
	MultiObject(const SocketDevice &_rsd);
	//! Destructor
	~MultiObject();
	//! Returns true if the socket on position _pos is ok
	bool socketOk(uint _pos)const;
	
	//! Asynchronous accept an incomming connection
	int socketAccept(uint _pos, SocketDevice &_rsd);
	
	//! Asynchronous connect
	/*!
		\param _pos The socket identifier
		\param _rai An AddrInfo iterator holding the destination address.
	*/
	int socketConnect(uint _pos, const AddrInfoIterator& _rai);
	
	//! Asynchronous send for socket on position _pos
	int socketSend(uint _pos, const char* _pb, uint32 _bl, uint32 _flags = 0);
	//! Asynchronous send for socket on position _pos
	int socketSendTo(uint _pos, const char* _pb, uint32 _bl, const SockAddrPair &_sap, uint32 _flags = 0);
	//! Asynchronous receive for socket on position _pos
	int socketRecv(uint _pos, char *_pb, uint32 _bl, uint32 _flags = 0);
	//! Asynchronous receive for socket on position _pos
	int socketRecvFrom(uint _pos, char *_pb, uint32 _bl, uint32 _flags = 0);
	
	//! Get the size of the received data for socket on position _pos
	/*!
		Call this on successful completion of socketRecv
	*/
	uint32 socketRecvSize(uint _pos)const;
	//! Gets the peer (sender) address of the received data
	/*!
		Call this on successful completion of socketRecv
	*/
	const SockAddrPair &socketRecvAddr(uint _pos) const;
	
	
	//! The ammount of data sent on socket on position _pos
	const uint64& socketSendCount(uint _pos)const;
	//! The ammount of data received on socket on position _pos
	const uint64& socketRecvCount(uint _pos)const;
	//! Return true if the socket is already waiting for a send completion
	bool socketHasPendingSend(uint _pos)const;
	//! Return true if the socket is already waiting for a receive completion
	bool socketHasPendingRecv(uint _pos)const;
	
	//! Get the local address of the socket on position _pos
	int socketLocalAddress(uint _pos, SocketAddress &_rsa)const;
	//! Get the remote address of the socket on position _pos
	int socketRemoteAddress(uint _pos, SocketAddress &_rsa)const;
	
	//! Set the timeout for asynchronous opperation completion for the socket on position _pos
	void socketTimeout(uint _pos, const TimeSpec &_crttime, ulong _addsec, ulong _addnsec = 0);
	
	//! Gets the mask with completion events for the socket on position _pos
	uint32 socketEvents(uint _pos)const;
	
	//! Erase the socket on position _pos - call socketRequestRegister before
	void socketErase(uint _pos);
	//! Insert a new socket given an aio::Socket
	/*!
		\retval  < 0 on error, >=0 on success, meaning the position of the socket
	*/
	int socketInsert(Socket *_psock);
	//! Insert a new socket given an aio::Socket
	/*!
		\retval  < 0 on error, >=0 on success, meaning the position of the socket
	*/
	int socketInsert(const SocketDevice &_rsd);
	
	//! Request for registering the socket onto the aio::Selector
	void socketRequestRegister(uint _pos);
	//! Request for unregistering the socket from the aio::Selector
	/*!
		The sockets are automatically unregistered and erased
		on object destruction.
	*/
	void socketRequestUnregister(uint _pos);
	
	//! Gets the state associated to the socket on position _pos
	int socketState(unsigned _pos)const;
	//! Sets the state associated to the socket on position _pos
	void socketState(unsigned _pos, int _st);
	
	//! Returns true if the socket on position _pos is secured
	bool socketIsSecure(uint _pos)const;
	
	//! Gets the secure socket associated to socket on position _pos
	SecureSocket* socketSecureSocket(unsigned _pos);
	//! Sets the secure socket associated to socket on position _pos
	void socketSecureSocket(unsigned _pos, SecureSocket *_pss);
	//! Asynchronous secure accept
	int socketSecureAccept(unsigned _pos);
	//! Asynchronous secure connect
	int socketSecureConnect(unsigned _pos);
private:
	void reserve(uint _cp);//using only one allocation sets the pointers from the aioobject
	uint dataSize(uint _cp);
	uint newStub();
private:
	typedef Stack<uint>		PositionStackTp;
	
	PositionStackTp		posstk;
};


}//namespace aio

}//namespace foundation

#endif
