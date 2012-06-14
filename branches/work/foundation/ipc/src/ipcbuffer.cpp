/* Implementation file ipcbuffer.cpp
	
	Copyright 2012 Valentin Palade
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ipcbuffer.hpp"
#include "foundation/ipc/ipcservice.hpp"
#include "system/specific.hpp"
#include "system/exception.hpp"
#include "system/debug.hpp"

#include <cstring>
#include <ostream>

namespace fdt = foundation;

namespace foundation{
namespace ipc{
//---------------------------------------------------------
#ifdef NINLINES
#include "ipcbuffer.ipp"
#endif
//---------------------------------------------------------
/*static*/ char* Buffer::allocate(){
	return Specific::popBuffer(Specific::capacityToIndex(Capacity));
}

/*static*/ void Buffer::deallocate(char *_buf){
	Specific::pushBuffer(_buf, Specific::capacityToIndex(Capacity));
}
//---------------------------------------------------------------------
void Buffer::clear(){
	if(pb){
		Specific::pushBuffer(pb, Specific::capacityToIndex(bc));
		pb = NULL;
		bc = 0;
	}
}
//---------------------------------------------------------------------
Buffer::~Buffer(){
	clear();
}
//---------------------------------------------------------------------
void Buffer::optimize(uint16 _cp){
	const uint32	bufsz(this->bufferSize());
	const uint		id(Specific::sizeToIndex(bufsz));
	const uint		mid(Specific::capacityToIndex(_cp ? _cp : Buffer::Capacity));
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

bool Buffer::compress(Controller &_rctrl){
	const uint32 	updatesz = updateCount() * sizeof(uint32);
	const uint32	headersize = headerSize() - updatesz;
	BufferContext	bctx(headersize);
	int32			datasz(dataSize() + updatesz);
	char			*pd(data() - updatesz);
	
	uint32			tmpdatasz(datasz);
	char			*tmppd(pd);
	
	vdbgx(Dbg::ipc, "buffer before compress id = "<<this->id()<<" dl = "<<this->dl<<" size = "<<bufferSize());
	if(_rctrl.compressBuffer(bctx, this->bufferSize(), tmppd, tmpdatasz)){
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
		vdbgx(Dbg::ipc, "buffer success compress id = "<<this->id()<<" dl = "<<this->dl<<" size = "<<bufferSize());
	}else{
		if(bctx.reqbufid != (uint)-1){
			if(pd == tmppd){
				THROW_EXCEPTION("Allocated buffer must be returned even on no compression");
			}
			tmppd -= bctx.offset;
			Specific::pushBuffer(tmppd, bctx.reqbufid);
		}
		vdbgx(Dbg::ipc, "buffer failed compress id = "<<this->id()<<" dl = "<<this->dl<<" size = "<<bufferSize());
	}
	optimize();
	return true;
}

bool Buffer::decompress(Controller &_rctrl){
	if(!(flags() & CompressedFlag)){
		return true;
	}
	const uint32 	updatesz = updateCount() * sizeof(uint32);
	const uint32	headersize = headerSize() - updatesz;
	BufferContext	bctx(headersize);
	uint32			datasz = dl + updatesz;
	char			*pd = pb + headersize;
	
	uint32			tmpdatasz = datasz;
	char			*tmppd(pd);
	
	vdbgx(Dbg::ipc, "buffer before decompress id = "<<this->id()<<" dl = "<<this->dl<<" size = "<<bufferSize());
	if(_rctrl.decompressBuffer(bctx, tmppd, tmpdatasz)){
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
		vdbgx(Dbg::ipc, "buffer success decompress id = "<<this->id()<<" dl = "<<this->dl<<" size = "<<bufferSize());
	}else{
		//decompression failed
		if(bctx.reqbufid != (uint)-1){
			if(pd == tmppd){
				THROW_EXCEPTION("Allocated buffer must be returned even on failed compression");
			}
			tmppd -= bctx.offset;
			Specific::pushBuffer(tmppd, bctx.reqbufid);
		}
		vdbgx(Dbg::ipc, "buffer failed decompress id = "<<this->id()<<" dl = "<<this->dl<<" size = "<<bufferSize());
		return false;
	}
	return true;
}

bool Buffer::check()const{
	if(this->pb){
		if(headerSize() < sizeof(BaseSize)) return false;
		if(headerSize() > Capacity) return false;
		return true;
	}
	return false;
}

//---------------------------------------------------------------------
std::ostream& operator<<(std::ostream &_ros, const Buffer &_rb){
	_ros<<"BUFFER(";
	_ros<<" type = ";
	switch(_rb.type()){
		case Buffer::KeepAliveType: _ros<<"KeepAliveType";break;
		case Buffer::DataType: _ros<<"DataType";break;
		case Buffer::ConnectingType: _ros<<"ConnectingType";break;
		case Buffer::AcceptingType: _ros<<"AcceptingType";break;
		case Buffer::Unknown: _ros<<"Unknown";break;
		default: _ros<<"[INVALID TYPE]";
	}
	_ros<<" id = "<<_rb.id();
	_ros<<" retransmit = "<<(int)_rb.resend();
	_ros<<" flags = "<<(int)_rb.flags();
	if(_rb.flags() & Buffer::RelayFlag){
		_ros<<" relay = "<<_rb.relay();
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
}//namespace foundation
