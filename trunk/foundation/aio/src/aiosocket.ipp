/* Inline implementation file aiosocket.ipp
	
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

#ifndef UINLINES
#define inline
#else
#include "clientserver/aio/src/aiosocket.hpp"
#endif

inline bool Socket::isSecure()const{
	return pss != NULL;
}
inline bool Socket::ok()const{
	return sd.ok();
}
inline int Socket::send(const char* _pb, uint32 _bl, uint32 _flags){
	if(pss == NULL){
		return doSendPlain(_pb, _bl, _flags);
	}else{
		return doSendSecure(_pb, _bl, _flags);
	}
}
inline int Socket::recv(char *_pb, uint32 _bl, uint32 _flags){
	if(pss == NULL){
		return doRecvPlain(_pb, _bl, _flags);
	}else{
		return doRecvSecure(_pb, _bl, _flags);
	}
}

inline int Socket::doSend(){
	if(pss == NULL){
		return doSendPlain();
	}else{
		return doSendSecure();
	}
}
inline int Socket::doRecv(){
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
#ifndef UINLINES
#undef inline
#endif

