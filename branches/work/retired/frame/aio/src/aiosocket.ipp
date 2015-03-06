// frame/aio/src/aiosocket.ipp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifdef NINLINES
#define inline
#else
#include "frame/aio/src/aiosocket.hpp"
#endif

inline bool Socket::isSecure()const{
	return pss != NULL;
}
inline bool Socket::ok()const{
	return sd.ok();
}
inline AsyncE Socket::send(const char* _pb, uint32 _bl, uint32 _flags){
	if(pss == NULL){
		return doSendPlain(_pb, _bl, _flags);
	}else{
		return doSendSecure(_pb, _bl, _flags);
	}
}
inline AsyncE Socket::recv(char *_pb, uint32 _bl, uint32 _flags){
	if(pss == NULL){
		return doRecvPlain(_pb, _bl, _flags);
	}else{
		return doRecvSecure(_pb, _bl, _flags);
	}
}

inline ulong Socket::doSend(){
	if(pss == NULL){
		return doSendPlain();
	}else{
		return doSendSecure();
	}
}
inline ulong Socket::doRecv(){
	if(pss == NULL){
		return doRecvPlain();
	}else{
		return doRecvSecure();
	}
}

inline uint32 Socket::ioRequest()const{
	return ioreq;
}
inline SecureSocket* Socket::secureSocket()const{
	return pss;
}
#ifdef NINLINES
#undef inline
#endif

