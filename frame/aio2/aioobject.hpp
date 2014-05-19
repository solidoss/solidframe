// frame/aio/aioobject.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_OBJECT_HPP
#define SOLID_FRAME_AIO_OBJECT_HPP

#include "frame/object.hpp"
#include "frame/aio/aiocommon.hpp"
#include "system/socketdevice.hpp"
#include "system/error.hpp"

namespace solid{
namespace frame{
namespace aio{

class ObjectBase: public Dynamic<ObjectBase, frame::Object>{
protected:
private:
};

template <class Sock = PlainSocket, size_t SockCnt = 1>
class Object;

template <class Sock>
class Object<Sock, 1>: public Dynamic<Object<Sock, 1>, ObjectBase>{
public:
	typedef Object<Sock, 1>		ThisT;
	
	static ThisT& specific(){
		return static_cast<ThisT&>(Object::specific());
	}
	Object();
	
	Object(const SocketDevice &_rsd, const bool _isacceptor = false);
	
	//! Destructor
	~Object();
	
	bool socketReinit(const SocketDevice &_rsd = SocketDevice());
	
	//! Returs true if the socket is ok
	bool socketOk()const;
	
	//! Asynchronous accept an incomming connection
	AsyncE socketAccept(
		SocketDevice &_rsd,
		ERROR_NS::error_code *_preterr = NULL
	);
	
	//! Asynchronous connect
	/*!
		\param _rai An SocketAddressInfo iterator holding the destination address.
	*/
	AsyncE socketConnect(
		const SocketAddressStub& _rsas,
		ERROR_NS::error_code *_preterr = NULL
	);
	
	//! Asynchronous send
	AsyncE socketSendAll(
		const char* _pb, const size_t _bcp,
		ERROR_NS::error_code *_preterr = NULL,
		uint32 _flags = 0
	);
	
	//! Asynchronous send to a specific address
	AsyncE socketSendTo(
		const char *_pb, const size_t _bcp,
		const SocketAddressStub &_sap,
		ERROR_NS::error_code *_preterr = NULL,
		uint32 _flags = 0
	);
	
	//! Asynchronous receive
	AsyncE socketRecvSome(
		char *_pb, const size_t _bcp,
		size_t &_rrecvsz,
		ERROR_NS::error_code *_preterr = NULL,
		uint32 _flags = 0
	);
	
	//! Asynchronous receive
	AsyncE socketRecvFrom(
		char *_pb, const size_t _bcp,
		size_t &_rrecvsz,
		SocketAddressInet &_rrecvaddr,
		ERROR_NS::error_code *_preterr = NULL,
		uint32 _flags = 0
	);
	
		//! Get the local address of the socket
	bool socketLocalAddress(SocketAddress &_rsa, ERROR_NS::error_code *_preterr = NULL)const;
	//! Get the remote address of the socket
	bool socketRemoteAddress(SocketAddress &_rsa, ERROR_NS::error_code *_preterr = NULL)const;
	
	//! Grabs the internal aio socket structure - to move it to another aioobject
	void socketGrab(SocketDevice &_rsd);
	
	//! Asynchronous secure accept
	AsyncE socketSecureAccept();
	//! Asynchronous secure connect
	AsyncE socketSecureConnect();
private:
};

template <class Sock>
class Object<Sock, 0>: public Dynamic<Object<Sock, 0>, ObjectBase>{
public:
	typedef Object<Sock, 0>		ThisT;
	
	static ThisT& specific(){
		return static_cast<ThisT&>(Object::specific());
	}
	
};

template <class Sock, size_t SockCnt>
class Object: public Dynamic<Object<Sock, SockCnt>, ObjectBase>{
public:
	typedef Object<Sock, SockCnt>		ThisT;
	
	static ThisT& specific(){
		return static_cast<ThisT&>(Object::specific());
	}
	
};


class Object: public Dynamic<SingleObject, Object>{
public:
	static Object& specific();
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


}//namespace aio
}//namespace frame
}//namespace solid


#endif
