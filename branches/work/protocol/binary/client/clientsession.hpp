// protocol/binary/client/clientsession.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_PROTOCOL_BINARY_CLIENT_SESSION_HPP
#define SOLID_PROTOCOL_BINARY_CLIENT_SESSION_HPP

#include <deque>

#include "utility/dynamicpointer.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"

namespace solid{
namespace protocol{
namespace binary{
namespace client{

struct BasicController{
	
};

template <class Msg, class MsgCtx, class Ctl = BasicController>
class Session{
	struct MessageStub{
		MessageStub():prcvmsg(NULL), sndflgs(0){}
		DynamicPointer<Msg>	sndmsgptr;
		MsgCtx				msgctx;
		Msg					*prcvmsg;
		uint32				sndflgs;
	};
public:
	Session():rcvbufoff(0){}
	
	template <class T>
	Session(const T &_rt):ctl(_rt), rcvbufoff(0){
		
	}
	
	size_t send(DynamicPointer<Msg> &_rmsgptr, uint32 _flags = 0){
		size_t	idx;
		if(freestk.size()){
			idx = freestk.top();
			freestk.pop();
		}else{
			idx = msgvec.size();
			msgvec.push_back(MessageStub());
		}
		MessageStub &rms = msgvec[idx];
		rms.sndflgs = _flags;
		rms.sndmsgptr = _rmsgptr;
		return idx;
	}
	
	size_t send(DynamicPointer<Msg> &_rmsgptr, MsgCtx &_rmsgctx, uint32 _flags = 0){
		size_t	idx;
		if(freestk.size()){
			idx = freestk.top();
			freestk.pop();
		}else{
			idx = msgvec.size();
			msgvec.push_back(MessageStub());
		}
		MessageStub &rms = msgvec[idx];
		rms.sndflgs = _flags;
		rms.msgctx = _rmsgctx;
		rms.sndmsgptr = _rmsgptr;
		return idx;
	}
	
	MsgCtx& messageContext(const size_t _idx){
		return msgvec[_idx].msgctx;
	}
	
	const MsgCtx& messageContext(const size_t _idx)const{
		return msgvec[_idx].msgctx;
	}
	
	char * recvBufferOffset(char *_pbuf)const{
		return _pbuf + rcvbufoff;
	}
	const size_t recvBufferCapacity(const size_t _cp)const{
		return _cp - rcvbufoff;
	}
	template <class Iter>
	void send(Iter _beg, const Iter _end){
		while(_beg != _end){
			send(_beg->first, _beg->second);
			++_beg;
		}
	}
	template <class C>
	bool consume(const char *_pb, size_t _bl, C &_rc, char *_tmpbuf, const size_t _tmpbufcp){
		return false;
	}
	template <class C>
	int fill(char *_pb, size_t _bl, C &_rc, char *_tmpbuf, const size_t _tmpbufcp){
		return NOK;
	}
private:
	typedef std::deque<MessageStub>		MessageVectorT;
	typedef Queue<size_t>				SizeTQueueT;
	typedef Stack<size_t>				SizeTStackT;
	
	uint16				rcvbufoff;
	Ctl					ctl;
	MessageVectorT		msgvec;
	SizeTQueueT			sndq;
	SizeTStackT			freestk;
};

}//namespace client
}//namespace binary
}//namespace protocol
}//namespace solid


#endif
