/* Implementation file aiomultiobject.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FOUNDATION_AIO_MULTI_OBJECT_HPP
#define FOUNDATION_AIO_MULTI_OBJECT_HPP

#include "foundation/aio/aioobject.hpp"
#include "foundation/aio/aiosocketpointer.hpp"
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

class MultiObject: public Dynamic<MultiObject, Object>{
public:
	//! Constructor given an aio::Socket (it will be inserted on position zero)
	MultiObject(const SocketPointer& _psock = SocketPointer());
	//! Constructor given a SocketDevice (it will be inserted on position zero)
	MultiObject(const SocketDevice &_rsd);
	//! Destructor
	~MultiObject();
	
	static MultiObject& the();
	
	uint count()const;
	
	uint signaledSize()const;
	uint signaledFront()const;
	void signaledPop();
	
	//! Returns true if the socket on position _pos is ok
	bool socketOk(const uint _pos)const;
	
	//! Asynchronous accept an incomming connection
	int socketAccept(const uint _pos, SocketDevice &_rsd);
	
	//! Asynchronous connect
	/*!
		\param _pos The socket identifier
		\param _rai An AddrInfo iterator holding the destination address.
	*/
	int socketConnect(const uint _pos, const AddrInfoIterator& _rai);
	
	//! Asynchronous send for socket on position _pos
	int socketSend(const uint _pos, const char* _pb, uint32 _bl, uint32 _flags = 0);
	//! Asynchronous send for socket on position _pos
	int socketSendTo(const uint _pos, const char* _pb, uint32 _bl, const SockAddrPair &_sap, uint32 _flags = 0);
	//! Asynchronous receive for socket on position _pos
	int socketRecv(const uint _pos, char *_pb, uint32 _bl, uint32 _flags = 0);
	//! Asynchronous receive for socket on position _pos
	int socketRecvFrom(const uint _pos, char *_pb, uint32 _bl, uint32 _flags = 0);
	
	//! Get the size of the received data for socket on position _pos
	/*!
		Call this on successful completion of socketRecv
	*/
	uint32 socketRecvSize(const uint _pos)const;
	//! Gets the peer (sender) address of the received data
	/*!
		Call this on successful completion of socketRecv
	*/
	const SockAddrPair &socketRecvAddr(uint _pos) const;
	
	
	//! The ammount of data sent on socket on position _pos
	const uint64& socketSendCount(const uint _pos)const;
	//! The ammount of data received on socket on position _pos
	const uint64& socketRecvCount(const uint _pos)const;
	//! Return true if the socket is already waiting for a send completion
	bool socketHasPendingSend(const uint _pos)const;
	//! Return true if the socket is already waiting for a receive completion
	bool socketHasPendingRecv(const uint _pos)const;
	
	//! Get the local address of the socket on position _pos
	int socketLocalAddress(const uint _pos, SocketAddress &_rsa)const;
	//! Get the remote address of the socket on position _pos
	int socketRemoteAddress(const uint _pos, SocketAddress &_rsa)const;
	
	//! Set the timeout for asynchronous recv opperation completion for the socket on position _pos
	void socketTimeoutRecv(const uint _pos, const TimeSpec &_crttime, ulong _addsec, ulong _addnsec = 0);
	//! Set the timeout for asynchronous send opperation completion for the socket on position _pos
	void socketTimeoutSend(const uint _pos, const TimeSpec &_crttime, ulong _addsec, ulong _addnsec = 0);
	
	
	//! Set the timeout for asynchronous recv opperation completion for the socket on position _pos
	void socketTimeoutRecv(const uint _pos, ulong _addsec, ulong _addnsec = 0);
	//! Set the timeout for asynchronous send opperation completion for the socket on position _pos
	void socketTimeoutSend(const uint _pos, ulong _addsec, ulong _addnsec = 0);
	
	//! Gets the mask with completion events for the socket on position _pos
	uint32 socketEvents(const uint _pos)const;
	
	//! Erase the socket on position _pos - call socketRequestRegister before
	void socketErase(const uint _pos);
	
	//! Grabs the aio internal socket structure - for moving it to another aioobject
	void socketGrab(const uint _pos, SocketPointer &_rsp);
	//! Insert a new socket given an aio::Socket
	/*!
		\retval  < 0 on error, >=0 on success, meaning the position of the socket
	*/
	int socketInsert(const SocketPointer &_rsp);
	//! Insert a new socket given an aio::Socket
	/*!
		\retval  < 0 on error, >=0 on success, meaning the position of the socket
	*/
	int socketInsert(const SocketDevice &_rsd);
	
	//! Request for registering the socket onto the aio::Selector
	void socketRequestRegister(const uint _pos);
	//! Request for unregistering the socket from the aio::Selector
	/*!
		The sockets are automatically unregistered and erased
		on object destruction.
	*/
	void socketRequestUnregister(const uint _pos);
	
	//! Gets the state associated to the socket on position _pos
	int socketState(const uint _pos)const;
	//! Sets the state associated to the socket on position _pos
	void socketState(const uint _pos, int _st);
	
	//! Returns true if the socket on position _pos is secured
	bool socketIsSecure(const uint _pos)const;
	
	//! Gets the secure socket associated to socket on position _pos
	SecureSocket* socketSecureSocket(uint _pos);
	//! Sets the secure socket associated to socket on position _pos
	void socketSecureSocket(const uint _pos, SecureSocket *_pss);
	//! Asynchronous secure accept
	int socketSecureAccept(const uint _pos);
	//! Asynchronous secure connect
	int socketSecureConnect(const uint _pos);
private:
	void reserve(const uint _cp);//using only one allocation sets the pointers from the aioobject
	uint dataSize(const uint _cp);
	uint newStub();
private:
	typedef Stack<uint>		PositionStackT;
	
	PositionStackT		posstk;
	int32				respoppos;
};

inline uint MultiObject::count()const{
	return this->stubcp - this->posstk.size();
}

inline void MultiObject::socketTimeoutRecv(const uint _pos, ulong _addsec, ulong _addnsec){
	socketTimeoutRecv(_pos, Object::currentTime(), _addsec, _addnsec);
}
inline void MultiObject::socketTimeoutSend(const uint _pos, ulong _addsec, ulong _addnsec){
	socketTimeoutSend(_pos, Object::currentTime(), _addsec, _addnsec);
}

/*static*/ inline MultiObject& MultiObject::the(){
	return static_cast<MultiObject&>(Object::the());
}

}//namespace aio

}//namespace foundation

#endif
