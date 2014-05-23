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
#include "frame/aio2/aiocommon.hpp"
#include "system/socketdevice.hpp"
#include "system/error.hpp"
#include "aioplainsocket.hpp"

namespace solid{
namespace frame{
namespace aio{

class ObjectBase: public Dynamic<ObjectBase, frame::Object>{
public:
	size_t signaledSocketId()const;
protected:
private:
	friend struct BaseSocket;
	virtual size_t registerSocket(BaseSocket*, const size_t);
protected:
	std::vector<BaseSocket*>		sockvec;
	size_t							crtsockidx;
};

template <size_t SockCnt = 1>
class Object;


template <>
class Object<0>: public Dynamic<Object<0>, ObjectBase>{
public:
	typedef Object<0>		ThisT;
	
	static ThisT& specific(){
		return static_cast<ThisT&>(Object::specific());
	}
private:
	
};

template <size_t SockCnt>
class Object: public Dynamic<Object<SockCnt>, ObjectBase>{
public:
	typedef Object<SockCnt>		ThisT;
	
	static ThisT& specific(){
		return static_cast<ThisT&>(Object::specific());
	}
private:
	BaseSocket						*sockarr[SockCnt];
	std::vector<BaseSocket*>		sockvec;
};

#if 0
template <class Sock, size_t SockCnt>
class Object: public Dynamic<Object<Sock, 1>, ObjectBase>{
public:
	typedef Object<Sock, SockCnt>		ThisT;
	
	static ThisT& specific(){
		return static_cast<ThisT&>(Object::specific());
	}
	Object();
	
	Object(const SocketDevice &_rsd){
		socketReinit(0, _rsd);
	}
	
	//! Destructor
	~Object();
	
	void socketReinit(const size_t _idx, const SocketDevice &_rsd = SocketDevice()){
		sv[_idx].device(_rsd);
	}
	
	bool socketRegister(const size_t _idx, const SocketDevice &_rsd = SocketDevice()){
		sv[_idx].registerDevice(_idx, _rsd);
	}
	
	//! Returs true if the socket is ok
	bool socketOk(const size_t _idx)const{
		return sv[_idx].device().ok();
	}
	
	//! Asynchronous accept an incomming connection
	AsyncE socketAccept(
		const size_t _idx,
		SocketDevice &_rsd,
		ERROR_NS::error_code *_preterr = NULL
	){
		return sv[_idx].accept(_idx, _rsd, _preterr);
	}
	
	bool socketHasPendingSend(const size_t _idx)const{
		return sv[_idx].hasPendingSend();
	}
	bool socketHasPendingRecv(const size_t _idx)const{
		return sv[_idx].hasPendingRecv();
	}
	//! Asynchronous connect
	/*!
		\param _rai An SocketAddressInfo iterator holding the destination address.
	*/
	AsyncE socketConnect(
		const size_t _idx,
		const SocketAddressStub& _rsas,
		ERROR_NS::error_code *_preterr = NULL
	){
		return sv[_idx].connect(_idx, _rsas, _preterr);
	}
	
	//! Asynchronous send
	AsyncE socketSendAll(
		const size_t _idx,
		const char* _pb, const size_t _bcp,
		ERROR_NS::error_code *_preterr = NULL,
		uint32 _flags = 0
	){
		return sv[_idx].sendAll(_idx, _pb, _bcp, _preterr, _flags);
	}
	
	//! Asynchronous send to a specific address
	AsyncE socketSendTo(
		const size_t _idx,
		const char *_pb, const size_t _bcp,
		const SocketAddressStub &_sap,
		ERROR_NS::error_code *_preterr = NULL,
		uint32 _flags = 0
	){
		return sv[_idx].sendTo(_idx, _pb, _bcp, _sap, _preterr, _flags);
	}
	
	//! Asynchronous receive
	AsyncE socketRecvSome(
		const size_t _idx,
		char *_pb, const size_t _bcp,
		size_t &_rrecvsz,
		ERROR_NS::error_code *_preterr = NULL,
		uint32 _flags = 0
	){
		
	}
	
	//! Asynchronous receive
	AsyncE socketRecvFrom(
		const size_t _idx,
		char *_pb, const size_t _bcp,
		size_t &_rrecvsz,
		SocketAddressInet &_rrecvaddr,
		ERROR_NS::error_code *_preterr = NULL,
		uint32 _flags = 0
	);
	
		//! Get the local address of the socket
	bool socketLocalAddress(
		const size_t _idx,
		SocketAddress &_rsa, ERROR_NS::error_code *_preterr = NULL
	)const;
	//! Get the remote address of the socket
	bool socketRemoteAddress(
		const size_t _idx, 
		SocketAddress &_rsa, ERROR_NS::error_code *_preterr = NULL
	)const;
	
	//! Grabs the internal aio socket structure - to move it to another aioobject
	void socketGrab(const size_t _idx, SocketDevice &_rsd);
	
	//! Asynchronous secure accept
	AsyncE socketSecureAccept(const size_t _idx);
	//! Asynchronous secure connect
	AsyncE socketSecureConnect(const size_t _idx);
private:
	/*virtual*/ void socketIoRun(const size_t _idx, const bool _hasread, const bool _haswrite){
		sv[_idx].ioRun(_idx, _hasread, _haswrite);
	}
	
	/*virtual*/ void socketRescheduled(const size_t _idx){
		
	}
	
	/*virtual*/ void clearSockets(){
		for(size_t i = 0; i < SockCnt; ++i){
			sv[i].clear();
		}
	}
private:
	Sock	sv[SockCnt];
};
#endif

}//namespace aio
}//namespace frame
}//namespace solid


#endif
