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
#include <cstring>

#include "utility/dynamicpointer.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"

namespace solid{
namespace protocol{
namespace binary{
namespace client{

struct PacketHeader{
	enum{
        SizeOf = 4,
        DataType = 1,
        CompressedFlag = 1
    };
	PacketHeader(
		const uint8 _type = 0,
		const uint8 _flags = 0,
		const uint16 _size = 0
	): type(_type), flags(_flags), size(_size){}
	void reset(
		const uint8 _type = 0,
		const uint8 _flags = 0,
		const uint16 _size = 0
	){
		type = _type;
		flags = _flags;
		size = _size;
	}
	
	bool isDataType()const{
        return type == DataType;
    }
    
    bool isCompressed()const{
        return flags & CompressedFlag;
    }
    template <class S>
    char* store(S &_rs, char * _pc)const{
		_pc = _rs.storeValue(_pc, type);
		_pc = _rs.storeValue(_pc, flags);
		_pc = _rs.storeValue(_pc, size);
		return _pc;
	}
	
	template <class D>
    const char* load(D &_rd, const char *_pc){
		_pc = _rd.loadValue(_pc, type);
		_pc = _rd.loadValue(_pc, flags);
		_pc = _rd.loadValue(_pc, size);
		return _pc;
	}
	
	uint8	type;
	uint8	flags;
	uint16	size;
};

struct BasicController{
	
};

template <class Msg, class MsgCtx, class Ctl = BasicController>
class Session{
	struct MessageStub{
		MessageStub():prcvmsg(NULL), sndflgs(0), onsendq(false){}
		DynamicPointer<Msg>	sndmsgptr;
		MsgCtx				msgctx;
		Msg					*prcvmsg;
		uint32				sndflgs;
		bool				onsendq;
	};
	enum{
		RecvPacketHeaderState = 1,
		RecvPacketDataState
	};
public:
	Session():rcvbufoff(0), rcvstate(RecvPacketHeaderState){}
	
	template <class T>
	Session(const T &_rt):ctl(_rt), rcvbufoff(0), rcvstate(RecvPacketHeaderState){
		
	}
	
	size_t send(const size_t _idx, DynamicPointer<Msg> &_rmsgptr, uint32 _flags = 0){
		if(_idx >= msgvec.size()){
			msgvec.resize(_idx + 1);
		}
		MessageStub &rms = msgvec[_idx];
		cassert(!rms.onsendq);
		rms.sndflgs = _flags;
		rms.sndmsgptr = _rmsgptr;
		rms.onsendq = true;
		sndq.push(_idx);
		return _idx;
	}
	
	size_t send(const size_t _idx, DynamicPointer<Msg> &_rmsgptr, MsgCtx &_rmsgctx, uint32 _flags = 0){
		if(_idx >= msgvec.size()){
			msgvec.resize(_idx + 1);
		}
		MessageStub &rms = msgvec[_idx];
		cassert(!rms.onsendq);
		rms.sndflgs = _flags;
		rms.msgctx = _rmsgctx;
		rms.sndmsgptr = _rmsgptr;
		rms.onsendq = true;
		sndq.push(_idx);
		return _idx;
	}
	
	DynamicPointer<Msg>& sendMessage(const size_t _idx){
		cassert(_idx < msgvec.size());
		return msgvec[_idx].sndmsgptr;
	}
	DynamicPointer<Msg> const& sendMessage(const size_t _idx)const{
		cassert(_idx < msgvec.size());
		return msgvec[_idx].sndmsgptr;
	}
	
	MsgCtx& messageContext(const size_t _idx){
		cassert(_idx < msgvec.size());
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
	
	template <class Des, class Ctx, class C>
	bool consume(
		Des &_rd,
		Ctx &_rctx,
		char *_pb, size_t _bl,
		C &_rc,
		char *_tmpbuf, const size_t _tmpbufcp
	){
		typedef C	CompressorT;
		typedef Des DeserializerT;
		
		if(!_bl) return true;
		
		rcvbufoff += _bl;
		const char	*cnspos = _pb + cnsbufoff;
		size_t		cnslen = rcvbufoff - cnsbufoff;
		
		while(cnslen){
			if(rcvstate == RecvPacketHeaderState){
				if(cnslen < PacketHeader::SizeOf){
					optimizeRecvBuffer(_pb);
					return true;
				}
				rcvstate = RecvPacketDataState;
			}
			PacketHeader	pkthdr;
			
			pkthdr.load(_rd, cnspos);
			
			if((pkthdr.size + PacketHeader::SizeOf) > cnslen){
				optimizeRecvBuffer(_pb);
				return true;//wait for more data
			}
			//we have the entire packet
			size_t			destsz = _tmpbufcp;
			if(!_rc.decompress(_tmpbuf, destsz, cnspos + PacketHeader::SizeOf, pkthdr.size)){
				return false;
			}
			
			const char		*crttmppos = _tmpbuf;
			size_t			crttmplen = destsz;
			while(crttmplen){
				if(_rd.empty()){
					crttmppos = _rd.loadValue(crttmppos, rcvmsgidx);
					crttmplen -= sizeof(uint32);
					MessageStub	&rms = msgvec[rcvmsgidx];
					_rd.push(rms.prcvmsg, "message");
				}
				int rv = _rd.run(crttmppos, crttmplen, _rctx);
				if(rv < 0){
					return false;
				}
				crttmppos += rv;
				crttmplen -= rv;
				//if(_rd.empty()){
					
			}
			rcvstate = RecvPacketHeaderState;
			cnspos += (PacketHeader::SizeOf + pkthdr.size);
			cnslen -= (PacketHeader::SizeOf + pkthdr.size);
		}
		optimizeRecvBuffer(_pb);
		return true;
	}
	
	template <class Ser, class Ctx, class C>
	int fill(
		Ser &_rs,
		Ctx &_rctx,
		char *_pb, size_t _bl,
		C &_rc,
		char *_tmpbuf, const size_t _tmpbufcp
	){
		typedef C	CompressorT;
		typedef Ser SerializerT;
		
		if(sndq.empty()) return 0;
		
		char 	*crtpos = _pb;
		size_t	crtlen = _bl;
		
		while(crtlen > _tmpbufcp && sndq.size()){
			char 	*crttmppos = _tmpbuf;
			size_t	crttmplen = _tmpbufcp;
			size_t	crttmpsz = 0;
			//crttmppos += PacketHeader::SizeOf;
			crttmplen -= PacketHeader::SizeOf;
			crttmplen -= _rc.reservedSize();
			
			while(crttmplen > 8 && sndq.size()){
				MessageStub		&rms = msgvec[sndq.front()];
				
				if(_rs.empty()){
					crttmppos = _rs.storeValue(crttmppos, static_cast<uint32>(sndq.front()));
					crttmplen -= sizeof(uint32);
					_rs.push(rms.sndmsgptr.get(), "message");
				}
				
				_rctx.messageIndex(sndq.front());
				
				int rv = _rs.run(crttmppos, crttmplen, _rctx);
				if(rv < 0){
					return -1;
				}
				
				crttmppos += rv;
				crttmplen -= rv;
				crttmpsz  += rv;
				
				if(_rs.empty()){
					sndq.pop();
				}
			}
			
			size_t 			destsz = crtlen - PacketHeader::SizeOf;
			
			if(!_rc.compress(crtpos + PacketHeader::SizeOf, destsz, _tmpbuf, crttmpsz)){
				return -1;
			}
			
			PacketHeader	pkthdr(PacketHeader::DataType, PacketHeader::CompressedFlag, destsz);
			
			crtpos = pkthdr.store(_rs, crtpos);
			crtlen -= PacketHeader::SizeOf;
			
			
			
			crtlen -= destsz;
			crtpos += destsz;
		}
		
		
		return crtpos - _pb;
	}
private:
	void optimizeRecvBuffer(char *_pb){
		const size_t cnssz = rcvbufoff - cnsbufoff;
		if(cnssz <= cnsbufoff){
			memcpy(_pb, _pb + cnsbufoff, cnssz);
			cnsbufoff = 0;
			rcvbufoff -= cnssz;
		}
	}
private:
	typedef std::deque<MessageStub>		MessageVectorT;
	typedef Queue<size_t>				SizeTQueueT;
	
	uint16				rcvbufoff;
	uint16				cnsbufoff;
	uint8				rcvstate;
	uint32				rcvmsgidx;
	Ctl					ctl;
	MessageVectorT		msgvec;
	SizeTQueueT			sndq;
};

}//namespace client
}//namespace binary
}//namespace protocol
}//namespace solid


#endif
