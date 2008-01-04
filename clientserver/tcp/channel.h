/* Declarations file channel.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CS_TCP_CHANNEL_H
#define CS_TCP_CHANNEL_H

#include <sys/epoll.h>

#include "system/socketaddress.h"
#include "utility/queue.h"
#include "clientserver/core/common.h"

class AddrInfoIterator;

class IStreamIterator;
class OStreamIterator;

namespace clientserver{
namespace tcp{

class ConnectionSelector;
class Station;
class SecureChannel;
struct ChannelData;

class Channel{
public:
	enum {
		RAISE_ON_END = 2, 
		TRY_ONLY = 4, 
		PREPARE_ONLY = 8};
	static Channel* create(const AddrInfoIterator &_rai);
	//Channel();
	~Channel();
	void prepare();
	void unprepare();
	int isOk()const;
	int connect(const AddrInfoIterator&);
	int isSecure();
	///send a buffer
	int send(const char* _pb, uint32 _bl, uint32 _flags = 0);
	///receives data into a buffer
	int recv(char *_pb, uint32 _bl, uint32 _flags = 0);
	///send a stream using a given buffer
	int send(IStreamIterator &_rsi, uint64 _sl, char *_pb, uint32 _bl, uint32 _flags = 0);
	///recv data into a stream using a given buffer
	int recv(OStreamIterator &_rsi, uint64 _sl, char *_pb, uint32 _bl, uint32 _flags = 0);
	///the size of the buffer received
	uint32 recvSize()const;
	const uint64& sendCount()const;
	const uint64& recvCount()const;
	///return != 0 if there are pending sends
	int arePendingSends();
protected:
private:
	Channel(int _sd);
	enum {
		INTOUT = EPOLLIN, OUTTOUT = EPOLLOUT,
		MUST_START = 1 << 31
	};
	ulong ioRequest()const;
	int doSendPlain(const char*, uint32, uint32);
	int doSendSecure(const char*, uint32, uint32);
	int doRecvPlain(char*, uint32, uint32);
	int doRecvSecure(char*, uint32, uint32);
	int doSendPlain(IStreamIterator&, uint64 , char*, uint32, uint32);
	int doSendSecure(IStreamIterator&, uint64 , char*, uint32, uint32);
	int doRecvPlain(OStreamIterator&, uint64 , char*, uint32, uint32);
	int doRecvSecure(OStreamIterator&, uint64 , char*, uint32, uint32);
	//the private interface is visible to ConnectionSelector
	friend class ConnectionSelector;
	friend class Station;
	int doRecv();
	int doSend();
	int doRecvPlain();
	int doRecvSecure();
	int doSendPlain();
	int doSendSecure();
	int descriptor()const{return sd;}
private:
	enum {IS_BUFFER = 1};
	int 				sd;
	uint64				rcvcnt;
	uint64				sndcnt;
	ChannelData			*pcd;
	SecureChannel		*psch;
};

inline const uint64& Channel::recvCount()const{return rcvcnt;}
inline const uint64& Channel::sendCount()const{return sndcnt;}
inline int Channel::isOk()const{return sd >= 0;}

inline int Channel::send(const char* _pb, uint32 _blen, uint32 _flags){
	if(psch)return doSendSecure(_pb, _blen, _flags);
	else	return doSendPlain(_pb, _blen, _flags);
}
inline int Channel::recv(char* _pb, uint32 _blen, uint32 _flags){
	if(psch)return doRecvPlain(_pb, _blen, _flags);
	else	return doRecvPlain(_pb, _blen, _flags);
}

inline int Channel::send(IStreamIterator& _rsi, uint64 _slen, char* _pb, uint32 _blen, uint32 _flags){
	if(psch)return doSendSecure(_rsi, _slen, _pb, _blen, _flags);
	else	return doSendPlain(_rsi, _slen, _pb, _blen, _flags);
}

inline int Channel::recv(OStreamIterator& _rsi, uint64 _slen, char* _pb, uint32 _blen, uint32 _flags){
	if(psch)return doRecvSecure(_rsi, _slen, _pb, _blen, _flags);
	else	return doRecvPlain(_rsi, _slen, _pb, _blen, _flags);
}

inline int Channel::doRecv(){
	if(psch)return doRecvSecure();
	else	return doRecvPlain();
}

inline int Channel::doSend(){
	if(psch)return doSendSecure();
	else	return doSendPlain();
}


}//namespace tcp
}//namespace clientserver

#endif
