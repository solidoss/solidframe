// protocol/binary/binarysession.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_PROTOCOL_BINARY_SESSION_HPP
#define SOLID_PROTOCOL_BINARY_SESSION_HPP

#include <deque>
#include <cstring>

#include "system/debug.hpp"
#include "utility/dynamicpointer.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"

namespace solid{
namespace protocol{
namespace binary{

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

struct DummyCompressor{
	const size_t reservedSize()const{
		return 0;
	}
	bool shouldCompress(const size_t _sz)const{
		return false;
	}
	bool compress(char *_pdest, size_t &_destsz, const char *_psrc, const size_t _srcsz){
		return false;
	}
	bool decompress(char *_pdest, size_t &_destsz, const char *_psrc, const size_t _srcsz){
		return false;
	}
};


struct BasicController{
	template <class Ctx>
	void onDoneSend(Ctx &_rctx, const size_t _msgidx){}
	
	template <class Ctx>
	void onSend(Ctx &_rctx, const size_t _sz){}
	
	template <class Ctx>
	void onRecv(Ctx &_rctx, const size_t _sz){}
};

template <class Msg, class MsgCtx, class Ctl = BasicController>
class Session{
	struct MessageStub{
		MessageStub():sndflgs(0), onsendq(false){}
		
		void sendClear(){
			sndmsgptr.clear();
			sndflgs = 0;
			onsendq = false;
		}
		void recvClear(){
			rcvmsgptr.clear();
		}
		
		MsgCtx				ctx;
		DynamicPointer<Msg>	sndmsgptr;
		DynamicPointer<Msg>	rcvmsgptr;
		uint32				sndflgs;
		bool				onsendq;
		
	};
	enum{
		RecvPacketHeaderState = 1,
		RecvPacketDataState
	};
public:
	
	Session():rcvbufoff(0), cnsbufoff(0), rcvstate(RecvPacketHeaderState), rcvmsgidx(-1){}
	
	template <class T>
	Session(T &_rt):ctl(_rt), rcvbufoff(0), cnsbufoff(0), rcvstate(RecvPacketHeaderState), rcvmsgidx(-1){
		
	}
	
	size_t send(size_t _idx, DynamicPointer<Msg> &_rmsgptr, uint32 _flags = 0){
		if(_idx == static_cast<size_t>(-1)){
			_idx = msgvec.size();
		}
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
		rms.ctx = _rmsgctx;
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
	
	DynamicPointer<Msg>& recvMessage(const size_t _idx){
		cassert(_idx < msgvec.size());
		return msgvec[_idx].rcvmsgptr;
	}
	DynamicPointer<Msg> const& recvMessage(const size_t _idx)const{
		cassert(_idx < msgvec.size());
		return msgvec[_idx].rcvmsgptr;
	}
	
	MsgCtx& messageContext(const size_t _idx){
		cassert(_idx < msgvec.size());
		return msgvec[_idx].ctx;
	}
	
	const MsgCtx& messageContext(const size_t _idx)const{
		cassert(_idx < msgvec.size());
		return msgvec[_idx].ctx;
	}
	bool isSendQueueEmpty()const{
		return sndq.empty();
	}
	bool isFreeSend(const size_t _idx){
		if(_idx < msgvec.size()){
			MessageStub &rms = msgvec[_idx];
			return !rms.onsendq;
		}else return true;
	}
//protected:
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
			const char		*crttmppos = _tmpbuf;
			size_t			crttmplen;
			if(pkthdr.isCompressed()){
				if(!_rc.decompress(_tmpbuf, destsz, cnspos + PacketHeader::SizeOf, pkthdr.size)){
					return false;
				}
				crttmplen = destsz;
			}else{
				crttmppos = cnspos + PacketHeader::SizeOf;
				crttmplen = pkthdr.size;
			}
			
			while(crttmplen){
				if(_rd.empty()){
					crttmppos = _rd.loadValue(crttmppos, rcvmsgidx);
					crttmplen -= sizeof(uint32);
					if(rcvmsgidx >= msgvec.size()){
						msgvec.resize(rcvmsgidx + 1);
					}
					idbgx(Debug::proto_bin, "receive message on pos "<<rcvmsgidx);
					MessageStub	&rms = msgvec[rcvmsgidx];
					_rd.push(rms.rcvmsgptr, "message");
				}
				
				_rctx.recvMessageIndex(rcvmsgidx);
				
				int rv = _rd.run(crttmppos, crttmplen, _rctx);
				
				if(rv < 0){
					msgvec[rcvmsgidx].recvClear();
					return false;
				}else if(_rd.empty()){
					msgvec[rcvmsgidx].recvClear();
				}
				crttmppos += rv;
				crttmplen -= rv;
			}
			rcvstate = RecvPacketHeaderState;
			cnspos += (PacketHeader::SizeOf + pkthdr.size);
			cnsbufoff += (PacketHeader::SizeOf + pkthdr.size);
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
		
		while(crtlen >= _tmpbufcp && sndq.size()){
			char 	*crttmppos = _tmpbuf;
			size_t	crttmplen = _tmpbufcp;
			size_t	crttmpsz = 0;
			crttmplen -= PacketHeader::SizeOf;
			crttmplen -= _rc.reservedSize();
			
			while(crttmplen > 8 && sndq.size()){
				const size_t	msgidx = sndq.front();
				MessageStub		&rms = msgvec[msgidx];
				
				if(_rs.empty()){
					crttmppos = _rs.storeValue(crttmppos, static_cast<uint32>(msgidx));
					crttmplen -= sizeof(uint32);
					crttmpsz  += sizeof(uint32);
					idbgx(Debug::proto_bin, "send message on pos "<<msgidx);
					_rs.push(rms.sndmsgptr.get(), "message");
				}
				
				_rctx.sendMessageIndex(sndq.front());
				
				int rv = _rs.run(crttmppos, crttmplen, _rctx);
				
				if(rv < 0){
					rms.sendClear();
					sndq.pop();
					ctl.onDoneSend(_rctx, msgidx);
					return -1;
				}else if(_rs.empty()){
					rms.sendClear();
					sndq.pop();
					ctl.onDoneSend(_rctx, msgidx);
				}
				
				crttmppos += rv;
				crttmplen -= rv;
				crttmpsz  += rv;
			}
			
			size_t 			destsz = crtlen - PacketHeader::SizeOf;
			uint8			pkgflags = 0;
			if(_rc.shouldCompress(crttmpsz)){
				if(!_rc.compress(crtpos + PacketHeader::SizeOf, destsz, _tmpbuf, crttmpsz)){
					return -1;
				}
				pkgflags |= PacketHeader::CompressedFlag;
			}else{
				memcpy(crtpos + PacketHeader::SizeOf, _tmpbuf, crttmpsz);
				destsz = crttmpsz;
			}
			
			PacketHeader	pkthdr(PacketHeader::DataType, pkgflags, destsz);
			
			crtpos = pkthdr.store(_rs, crtpos);
			crtlen -= PacketHeader::SizeOf;
			
			crtlen -= destsz;
			crtpos += destsz;
		}
		
		return crtpos - _pb;
	}
	template <class Ser, class Ctx>
	int fill(
		Ser &_rs,
		Ctx &_rctx,
		char *_pb, size_t _bl
	){
		typedef Ser SerializerT;
		
		if(sndq.empty()) return 0;
		
		char 	*crtpos = _pb;
		size_t	crtlen = _bl;
		
		char 	*crttmppos = crtpos + PacketHeader::SizeOf;
		size_t	crttmplen = crtlen - PacketHeader::SizeOf;
		size_t	crttmpsz = 0;
		
		while(crttmplen > 8 && sndq.size()){
			MessageStub		&rms = msgvec[sndq.front()];
			
			if(_rs.empty()){
				crttmppos = _rs.storeValue(crttmppos, static_cast<uint32>(sndq.front()));
				crttmplen -= sizeof(uint32);
				crttmpsz  += sizeof(uint32);
				_rs.push(rms.sndmsgptr.get(), "message");
			}
			
			_rctx.sendMessageIndex(sndq.front());
			
			int rv = _rs.run(crttmppos, crttmplen, _rctx);
			
			if(rv < 0){
				rms.sendClear();
				sndq.pop();
				ctl.onDoneSend(_rctx);
				return -1;
			}else if(_rs.empty()){
				rms.sendClear();
				sndq.pop();
				ctl.onDoneSend(_rctx);
			}
			
			crttmppos += rv;
			crttmplen -= rv;
			crttmpsz  += rv;
		}
		
		size_t 			destsz = crttmpsz;
		uint8			pkgflags = 0;
		PacketHeader	pkthdr(PacketHeader::DataType, pkgflags, destsz);
		
		crtpos = pkthdr.store(_rs, crtpos);
					
		//crtlen -= (destsz + PacketHeader::SizeOf);
		crtpos += destsz;
		
		return crtpos - _pb;
	}
private:
	void optimizeRecvBuffer(char *_pb){
		const size_t cnssz = rcvbufoff - cnsbufoff;
		if(cnssz <= cnsbufoff){
			idbgx(Debug::proto_bin, "memcopy "<<cnssz<<" rcvoff = "<<rcvbufoff<<" cnsoff = "<<cnsbufoff);
			memcpy(_pb, _pb + cnsbufoff, cnssz);
			cnsbufoff = 0;
			rcvbufoff = cnssz;
		}
	}
protected:
	Ctl					ctl;
private:
	typedef std::deque<MessageStub>		MessageVectorT;
	typedef Queue<size_t>				SizeTQueueT;
	
	uint16				rcvbufoff;
	uint16				cnsbufoff;
	uint8				rcvstate;
	uint32				rcvmsgidx;
	MessageVectorT		msgvec;
	SizeTQueueT			sndq;
};

}//namespace binary
}//namespace protocol
}//namespace solid


#endif
