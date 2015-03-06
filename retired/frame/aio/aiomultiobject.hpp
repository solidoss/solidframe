// frame/aio/aiomultiobject.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#ifndef SOLID_FRAME_AIO_MULTI_OBJECT_HPP
#define SOLID_FRAME_AIO_MULTI_OBJECT_HPP

#include "frame/aio/aiocommon.hpp"
#include "frame/aio/aioobject.hpp"
#include "frame/aio/aiosocketpointer.hpp"
#include "utility/stack.hpp"
#include "system/error.hpp"

namespace solid{

class SocketDevice;
class SocketAddress;
class SocketAddressStub;
class ResolveIterator;

namespace frame{
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
	MultiObject(const SocketDevice &_rsd, const bool _isacceptor = false);
	//! Destructor
	~MultiObject();
	
	static MultiObject& specific();
	
	size_t count()const;
	
	size_t signaledSize()const;
	size_t signaledFront()const;
	void signaledPop();
	
	//! Returns true if the socket on position _pos is ok
	bool socketOk(const size_t _pos)const;
	
	//! Asynchronous accept an incomming connection
	AsyncE socketAccept(const size_t _pos, SocketDevice &_rsd);
	
	//! Asynchronous connect
	/*!
		\param _pos The socket identifier
		\param _rai An SocketAddressInfo iterator holding the destination address.
	*/
	AsyncE socketConnect(const size_t _pos, const SocketAddressStub& _rsas);
	
	//! Asynchronous send for socket on position _pos
	AsyncE socketSend(const size_t _pos, const char* _pb, uint32 _bl, uint32 _flags = 0);
	//! Asynchronous send for socket on position _pos
	AsyncE socketSendTo(const size_t _pos, const char* _pb, uint32 _bl, const SocketAddressStub &_sap, uint32 _flags = 0);
	//! Asynchronous receive for socket on position _pos
	AsyncE socketRecv(const size_t _pos, char *_pb, uint32 _bl, uint32 _flags = 0);
	//! Asynchronous receive for socket on position _pos
	AsyncE socketRecvFrom(const size_t _pos, char *_pb, uint32 _bl, uint32 _flags = 0);
	
	//! Get the size of the received data for socket on position _pos
	/*!
		Call this on successful completion of socketRecv
	*/
	uint32 socketRecvSize(const size_t _pos)const;
	//! Gets the peer (sender) address of the received data
	/*!
		Call this on successful completion of socketRecv
	*/
	const SocketAddress &socketRecvAddr(const size_t _pos) const;
	
	
	//! The ammount of data sent on socket on position _pos
	const uint64& socketSendCount(const size_t _pos)const;
	//! The ammount of data received on socket on position _pos
	const uint64& socketRecvCount(const size_t _pos)const;
	//! Return true if the socket is already waiting for a send completion
	bool socketHasPendingSend(const size_t _pos)const;
	//! Return true if the socket is already waiting for a receive completion
	bool socketHasPendingRecv(const size_t _pos)const;
	
	//! Get the local address of the socket on position _pos
	bool socketLocalAddress(const size_t _pos, SocketAddress &_rsa)const;
	//! Get the remote address of the socket on position _pos
	bool socketRemoteAddress(const size_t _pos, SocketAddress &_rsa)const;
	
	//! Set the timeout for asynchronous recv opperation completion for the socket on position _pos
	void socketTimeoutRecv(const size_t _pos, const TimeSpec &_crttime, ulong _addsec, ulong _addnsec = 0);
	//! Set the timeout for asynchronous send opperation completion for the socket on position _pos
	void socketTimeoutSend(const size_t _pos, const TimeSpec &_crttime, ulong _addsec, ulong _addnsec = 0);
	
	
	//! Set the timeout for asynchronous recv opperation completion for the socket on position _pos
	void socketTimeoutRecv(const size_t _pos, ulong _addsec, ulong _addnsec = 0);
	//! Set the timeout for asynchronous send opperation completion for the socket on position _pos
	void socketTimeoutSend(const size_t _pos, ulong _addsec, ulong _addnsec = 0);
	
	//! Gets the mask with completion events for the socket on position _pos
	uint32 socketEvents(const size_t _pos)const;
	
	//! Erase the socket on position _pos - call socketRequestRegister before
	void socketErase(const size_t _pos);
	
	//! Grabs the aio internal socket structure - for moving it to another aioobject
	void socketGrab(const size_t _pos, SocketPointer &_rsp);
	//! Insert a new socket given an aio::Socket
	/*!
		\retval  < 0 on error, >=0 on success, meaning the position of the socket
	*/
	size_t socketInsert(const SocketPointer &_rsp);
	//! Insert a new socket given an aio::Socket
	/*!
		\retval  < 0 on error, >=0 on success, meaning the position of the socket
	*/
	size_t socketInsert(const SocketDevice &_rsd, const bool _isacceptor = false);
	
	//! Request for registering the socket onto the aio::Selector
	void socketRequestRegister(const size_t _pos);
	//! Request for unregistering the socket from the aio::Selector
	/*!
		The sockets are automatically unregistered and erased
		on object destruction.
	*/
	void socketRequestUnregister(const size_t _pos);
	
	//! Gets the state associated to the socket on position _pos
	int socketState(const size_t _pos)const;
	//! Sets the state associated to the socket on position _pos
	void socketState(const size_t _pos, int _st);
	
	//! Returns true if the socket on position _pos is secured
	bool socketIsSecure(const size_t _pos)const;
	
	//! Gets the secure socket associated to socket on position _pos
	SecureSocket* socketSecureSocket(const size_t _pos);
	//! Sets the secure socket associated to socket on position _pos
	void socketSecureSocket(const size_t _pos, SecureSocket *_pss);
	//! Asynchronous secure accept
	AsyncE socketSecureAccept(const size_t _pos);
	//! Asynchronous secure connect
	AsyncE socketSecureConnect(const size_t _pos);
	//TODO: not implemented
	ERROR_NS::error_code socketError(const size_t _pos)const;
private:
	void reserve(const size_t _cp);//using only one allocation sets the pointers from the aioobject
	size_t dataSize(const size_t _cp);
	size_t newStub();
private:
	typedef Stack<size_t>		PositionStackT;
	
	PositionStackT		posstk;
	size_t				respoppos;
};

inline size_t MultiObject::count()const{
	return this->stubcp - this->posstk.size();
}

inline void MultiObject::socketTimeoutRecv(const size_t _pos, ulong _addsec, ulong _addnsec){
	socketTimeoutRecv(_pos, Object::currentTime(), _addsec, _addnsec);
}
inline void MultiObject::socketTimeoutSend(const size_t _pos, ulong _addsec, ulong _addnsec){
	socketTimeoutSend(_pos, Object::currentTime(), _addsec, _addnsec);
}

/*static*/ inline MultiObject& MultiObject::specific(){
	return static_cast<MultiObject&>(Object::specific());
}

}//namespace aio
}//namespace frame
}//namespace solid

#endif
