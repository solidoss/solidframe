/* Implementation file aiosingleobject.hpp
	
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

#ifndef FOUNDATION_AIO_SINGLE_OBJECT_HPP
#define FOUNDATION_AIO_SINGLE_OBJECT_HPP

#include "foundation/aio/aioobject.hpp"
#include "foundation/aio/aiosocketpointer.hpp"

class SocketDevice;
class SocketAddress;
class SocketAddressPair;
class SocketAddressInfoIterator;


namespace foundation{


namespace aio{

class SecureSocket;

//! A class for single socket asynchronous communication
/*!
	Inherit this class if you need single socket communication.
	If your protocol need two or more socket to pass data to eachother
	use MultiObject instead.<br>
	All methods marked as asynchronous have three return values:<br>
	- BAD (-1) for unrecoverable error like socket connection closed;<br>
	- NOK (1) opperation is pending for completion within aio::Selector;<br>
	- OK (0) opperation completed successfully.<br>
*/
class SingleObject: public Dynamic<SingleObject, Object>{
public:
	static SingleObject& the();
	//! Constructor using an aio::Socket
	SingleObject(const SocketPointer& _psock = SocketPointer());
	//! Constructor using a SocketDevice
	SingleObject(const SocketDevice &_rsd);
	//! Destructor
	~SingleObject();
	//! Returs true if the socket is ok
	bool socketOk()const;
	
	//! Asynchronous accept an incomming connection
	int socketAccept(SocketDevice &_rsd);
	
	//! Asynchronous connect
	/*!
		\param _rai An SocketAddressInfo iterator holding the destination address.
	*/
	int socketConnect(const SocketAddressInfoIterator& _rai);
	
	//! Asynchronous send
	int socketSend(const char* _pb, uint32 _bl, uint32 _flags = 0);
	//! Asynchronous send to a specific address
	int socketSendTo(const char *_pb, uint32 _bl, const SocketAddressPair &_sap, uint32 _flags = 0);
	//! Asynchronous receive
	int socketRecv(char *_pb, uint32 _bl, uint32 _flags = 0);
	//! Asynchronous receive
	int socketRecvFrom(char *_pb, uint32 _bl, uint32 _flags = 0);
	//! Get the size of the received data
	/*!
		Call this on successful completion of socketRecv
	*/
	uint32 socketRecvSize()const;
	//! Gets the peer (sender) address of the received data
	/*!
		Call this on successful completion of socketRecv
	*/
	const SocketAddressPair &socketRecvAddr() const;
	
	
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
	
	//! Set the timeout for asynchronous recv opperation completion
	void socketTimeoutRecv(const TimeSpec &_crttime, ulong _addsec, ulong _addnsec = 0);
	//! Set the timeout for asynchronous send opperation completion
	void socketTimeoutSend(const TimeSpec &_crttime, ulong _addsec, ulong _addnsec = 0);
	
	
	//! Set the timeout for asynchronous recv opperation completion
	void socketTimeoutRecv(ulong _addsec, ulong _addnsec = 0);
	//! Set the timeout for asynchronous send opperation completion
	void socketTimeoutSend(ulong _addsec, ulong _addnsec = 0);
	
	//! Gets the mask with completion events for the socket.
	//uint32 socketEvents()const;
	
	uint32 socketEventsGrab();
	
	//! Erase the socket - call socketRequestRegister before
	void socketErase();
	
	//! Grabs the internal aio socket structure - to move it to another aioobject
	void socketGrab(SocketPointer &_rsp);
	
	//! Sets the socket given an aio::Socket
	int socketInsert(const SocketPointer &_rsp);
	//! Sets the socket given a SocketDevice
	int socketInsert(const SocketDevice &_rsd);
	
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
	
	
	//! Returns true if the socket is secure - SSL
	bool socketIsSecure()const;
	//! Gets the secure socket associated to socket
	SecureSocket* socketSecureSocket();
	//! Sets the secure socket associated to the socket
	void socketSecureSocket(SecureSocket *_pss);
	
	//! Asynchronous secure accept
	int socketSecureAccept();
	//! Asynchronous secure connect
	int socketSecureConnect();
	
private:
	SocketStub	stub;
	int32		req;
	int32		res;
	int32		itout;
	int32		otout;
};

inline void SingleObject::socketTimeoutRecv(ulong _addsec, ulong _addnsec){
	socketTimeoutRecv(Object::currentTime(), _addsec, _addnsec);
}
inline void SingleObject::socketTimeoutSend(ulong _addsec, ulong _addnsec){
	socketTimeoutSend(Object::currentTime(), _addsec, _addnsec);
}

/*static*/ inline SingleObject& SingleObject::the(){
	return static_cast<SingleObject&>(Object::the());
}


}//namespace aio

}//namespace foundation


#endif
