// frame/ipc/src/ipcpacket.cpp
//
// Copyright (c) 2012, 2013 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "ipcpacket.hpp"
#include "frame/ipc/ipcservice.hpp"
#include "system/specific.hpp"
#include "system/exception.hpp"
#include "system/debug.hpp"

#include <cstring>
#include <ostream>

namespace solid{
namespace frame{
namespace ipc{

//---------------------------------------------------------
#ifdef NINLINES
#include "ipcpacket.ipp"
#endif
//---------------------------------------------------------
/*static*/ char* Packet::allocate(){
	return Specific::popBuffer(Specific::capacityToIndex(Capacity));
}

/*static*/ void Packet::deallocate(char *_pb){
	Specific::pushBuffer(_pb, Specific::capacityToIndex(Capacity));
}
//---------------------------------------------------------------------
void Packet::clear(){
	if(pb){
		Specific::pushBuffer(pb, Specific::capacityToIndex(bc));
		pb = NULL;
		bc = 0;
	}
}
//---------------------------------------------------------------------
Packet::~Packet(){
	clear();
}
//---------------------------------------------------------------------
void Packet::optimize(uint16 _cp){
	const uint32	bufsz(this->bufferSize());
	const uint		id(Specific::sizeToIndex(bufsz));
	const uint		mid(Specific::capacityToIndex(_cp ? _cp : Packet::Capacity));
	if(mid > id){
		uint32 datasz = this->dataSize();//the size
		
		char *newbuf(Specific::popBuffer(id));
		memcpy(newbuf, this->buffer(), bufsz);//copy to the new buffer
		
		char *pb = this->release();
		Specific::pushBuffer(pb, mid);
		
		this->pb = newbuf;
		this->dl = datasz;
		this->bc = Specific::indexToCapacity(id);
	}
}

bool Packet::compress(Controller &_rctrl){
	const uint32 	updatesz = updateCount() * sizeof(uint32);
	const uint32	headersize = headerSize() - updatesz;
	PacketContext	bctx(headersize);
	int32			datasz(dataSize() + updatesz);
	char			*pd(data() - updatesz);
	
	uint32			tmpdatasz(datasz);
	char			*tmppd(pd);
	
	vdbgx(Debug::ipc, "buffer before compress id = "<<this->id()<<" dl = "<<this->dl<<" size = "<<bufferSize());
	if(_rctrl.compressPacket(bctx, this->bufferSize(), tmppd, tmpdatasz)){
		if(tmppd != pd){
			if(bctx.reqbufid == (uint)-1){
				THROW_EXCEPTION("Invalid buffer pointer returned");
			}
			tmppd -= bctx.offset;
			memcpy(tmppd, pb, headersize);
			Specific::pushBuffer(this->pb, Specific::capacityToIndex(this->bc));
			this->bc = Specific::indexToCapacity(bctx.reqbufid);
			this->pb = tmppd;
		}else{
			//the compression was done on-place
		}
		this->flags(this->flags() | CompressedFlag);
		this->dl -= (datasz - tmpdatasz);
		vdbgx(Debug::ipc, "buffer success compress id = "<<this->id()<<" dl = "<<this->dl<<" size = "<<bufferSize());
	}else{
		if(bctx.reqbufid != (uint)-1){
			if(pd == tmppd){
				THROW_EXCEPTION("Allocated buffer must be returned even on no compression");
			}
			tmppd -= bctx.offset;
			Specific::pushBuffer(tmppd, bctx.reqbufid);
		}
		vdbgx(Debug::ipc, "buffer failed compress id = "<<this->id()<<" dl = "<<this->dl<<" size = "<<bufferSize());
	}
	optimize();
	return true;
}

bool Packet::decompress(Controller &_rctrl){
	if(!(flags() & CompressedFlag)){
		return true;
	}
	const uint32 	updatesz = updateCount() * sizeof(uint32);
	const uint32	headersize = headerSize() - updatesz;
	PacketContext	bctx(headersize);
	uint32			datasz = dl + updatesz;
	char			*pd = pb + headersize;
	
	uint32			tmpdatasz = datasz;
	char			*tmppd(pd);
	
	vdbgx(Debug::ipc, "packet before decompress id = "<<this->id()<<" dl = "<<this->dl<<" size = "<<bufferSize());
	if(_rctrl.decompressPacket(bctx, tmppd, tmpdatasz)){
		if(bctx.reqbufid != (uint)-1){
			if(tmppd == pd){
				THROW_EXCEPTION("Requested buffer not returned");
			}
			tmppd -= bctx.offset;
			memcpy(tmppd, pb, headersize);
			Specific::pushBuffer(this->pb, Specific::capacityToIndex(this->bc));
			this->bc = Specific::indexToCapacity(bctx.reqbufid);
			this->pb = tmppd;
		}else{
			//on-place decompression
			if(tmppd != pd){
				THROW_EXCEPTION("Invalid buffer pointer returned");
			}
		}
		this->flags(this->flags() & (~CompressedFlag));
		this->dl += (tmpdatasz - datasz);
		vdbgx(Debug::ipc, "packet success decompress id = "<<this->id()<<" dl = "<<this->dl<<" size = "<<bufferSize());
	}else{
		//decompression failed
		if(bctx.reqbufid != (uint)-1){
			if(pd == tmppd){
				THROW_EXCEPTION("Allocated buffer must be returned even on failed compression");
			}
			tmppd -= bctx.offset;
			Specific::pushBuffer(tmppd, bctx.reqbufid);
		}
		vdbgx(Debug::ipc, "packet failed decompress id = "<<this->id()<<" dl = "<<this->dl<<" size = "<<bufferSize());
		return false;
	}
	return true;
}

bool Packet::check()const{
	if(this->pb){
		if(headerSize() < sizeof(BaseSize)) return false;
		if(headerSize() > Capacity) return false;
		return true;
	}
	return false;
}

//---------------------------------------------------------------------
std::ostream& operator<<(std::ostream &_ros, const Packet &_rb){
	_ros<<"PACKET(";
	_ros<<"type = ";
	switch(_rb.type()){
		case Packet::KeepAliveType: _ros<<"KeepAliveType";break;
		case Packet::DataType: _ros<<"DataType";break;
		case Packet::ConnectType: _ros<<"ConnectType";break;
		case Packet::AcceptType: _ros<<"AcceptType";break;
		case Packet::ErrorType: _ros<<"ErrorType";break;
		case Packet::Unknown: _ros<<"Unknown";break;
		default: _ros<<"[INVALID TYPE]";
	}
	_ros<<" id = "<<_rb.id();
	_ros<<" retransmit = "<<(int)_rb.resend();
	_ros<<" flags = "<<(int)_rb.flags();
	if(_rb.flags() & Packet::RelayFlag){
		_ros<<" relay = "<<_rb.relay()<<" relaysz = "<<_rb.relayPacketSize();
	}else{
		_ros<<" no_relay";
	}
	_ros<<" buffer_cp = "<<_rb.bufferCapacity();
	_ros<<" datalen = "<<_rb.dataSize();
	_ros<<" updatescnt = "<<(int)_rb.updateCount();
	_ros<<" updates [";
	for(int i = 0; i < _rb.updateCount(); ++i){
		_ros<<_rb.update(i)<<',';
	}
	_ros<<']';
	return _ros;
	
}


}//namespace ipc
}//namespace frame
}//namespace solid
