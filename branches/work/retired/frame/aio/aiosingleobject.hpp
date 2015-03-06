// frame/aio/aiosingleobject.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_SINGLE_OBJECT_HPP
#define SOLID_FRAME_AIO_SINGLE_OBJECT_HPP

#include "frame/aio/aiocommon.hpp"
#include "frame/aio/aioobject.hpp"
#include "frame/aio/aiosocketpointer.hpp"
#include "system/error.hpp"

namespace solid{

class SocketDevice;
class SocketAddress;
class SocketAddressStub;
class ResolveIterator;

namespace frame{
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
	static SingleObject& specific();
	//! Constructor using an aio::Socket
	SingleObject(const SocketPointer& _psock = SocketPointer());
	//! Constructor using a SocketDevice
	SingleObject(const SocketDevice &_rsd, const bool _isacceptor = false);
	//! Destructor
	~SingleObject();
	//! Returs true if the socket is ok
	bool socketOk()const;
	
	//! Asynchronous accept an incomming connection
	AsyncE socketAccept(SocketDevice &_rsd);
	
	//! Asynchronous connect
	/*!
		\param _rai An SocketAddressInfo iterator holding the destination address.
	*/
	AsyncE socketConnect(const SocketAddressStub& _rsas);
	
	//! Asynchronous send
	AsyncE socketSend(const char* _pb, uint32 _bl, uint32 _flags = 0);
	//! Asynchronous send to a specific address
	AsyncE socketSendTo(const char *_pb, uint32 _bl, const SocketAddressStub &_sap, uint32 _flags = 0);
	//! Asynchronous receive
	AsyncE socketRecv(char *_pb, uint32 _bl, uint32 _flags = 0);
	//! Asynchronous receive
	AsyncE socketRecvFrom(char *_pb, uint32 _bl, uint32 _flags = 0);
	//! Get the size of the received data
	/*!
		Call this on successful completion of socketRecv
	*/
	uint32 socketRecvSize()const;
	//! Gets the peer (sender) address of the received data
	/*!
		Call this on successful completion of socketRecv
	*/
	const SocketAddress &socketRecvAddr() const;
	
	
	//! The ammount of data sent
	const uint64& socketSendCount()const;
	//! The ammount of data received
	const uint64& socketRecvCount()const;
	//! Return true if the socket is already waiting for a send completion
	bool socketHasPendingSend()const;
	//! Return true if the socket is already waiting for a receive completion
	bool socketHasPendingRecv()const;
	
	//! Get the local address of the socket
	bool socketLocalAddress(SocketAddress &_rsa)const;
	//! Get the remote address of the socket
	bool socketRemoteAddress(SocketAddress &_rsa)const;
	
	//! Set the timeout for asynchronous recv opperation completion
	void socketTimeoutRecv(const TimeSpec &_crttime, const ulong _addsec, const ulong _addnsec = 0);
	//! Set the timeout for asynchronous send opperation completion
	void socketTimeoutSend(const TimeSpec &_crttime, const ulong _addsec, const ulong _addnsec = 0);
	
	
	//! Set the timeout for asynchronous recv opperation completion
	void socketTimeoutRecv(const ulong _addsec, const ulong _addnsec = 0);
	//! Set the timeout for asynchronous send opperation completion
	void socketTimeoutSend(const ulong _addsec, const ulong _addnsec = 0);
	
	//! Gets the mask with completion events for the socket.
	//uint32 socketEvents()const;
	
	uint32 socketEventsGrab();
	
	//! Erase the socket - call socketRequestRegister before
	void socketErase();
	
	//! Grabs the internal aio socket structure - to move it to another aioobject
	void socketGrab(SocketPointer &_rsp);
	
	//! Sets the socket given an aio::Socket
	bool socketInsert(const SocketPointer &_rsp);
	//! Sets the socket given a SocketDevice
	bool socketInsert(const SocketDevice &_rsd, const bool _isacceptor = false);
	
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
	AsyncE socketSecureAccept();
	//! Asynchronous secure connect
	AsyncE socketSecureConnect();
	
	//TODO: not implemented
	ERROR_NS::error_code socketError()const;
	
private:
	SocketStub	stub;
	size_t		req;
	size_t		res;
	size_t		itout;
	size_t		otout;
};

inline void SingleObject::socketTimeoutRecv(const ulong _addsec, const ulong _addnsec){
	socketTimeoutRecv(Object::currentTime(), _addsec, _addnsec);
}
inline void SingleObject::socketTimeoutSend(const ulong _addsec, const ulong _addnsec){
	socketTimeoutSend(Object::currentTime(), _addsec, _addnsec);
}

/*static*/ inline SingleObject& SingleObject::specific(){
	return static_cast<SingleObject&>(Object::specific());
}


}//namespace aio
}//namespace frame
}//namespace solid


#endif
