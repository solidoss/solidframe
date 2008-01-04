/* Declarations file iodata.h
	
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

#ifndef IPC_IODATA_H
#define IPC_IODATA_H

#include <cassert>
#include "system/socketaddress.h"

namespace clientserver{
class Visitor;
namespace udp{
class Station;
}
}

namespace serialization{
namespace bin{
class RTTIMapper;
}
}

namespace clientserver{
namespace ipc{

//*******	AddrPtrCmp		******************************************************************

struct Inet4AddrPtrCmp{
	bool operator()(const Inet4SockAddrPair*const &_sa1, const Inet4SockAddrPair*const &_sa2)const{
		//TODO: optimize
		assert(_sa1 && _sa2); 
		if(*_sa1 < *_sa2){
			return true;
		}else if(!(*_sa2 < *_sa1)){
			return _sa1->port() < _sa2->port();
		}
		return false;
	}
	typedef std::pair<const Inet4SockAddrPair*, int>	PairProcAddr;
	bool operator()(const PairProcAddr* const &_sa1, const PairProcAddr* const &_sa2)const{
		assert(_sa1 && _sa2); 
		assert(_sa1->first && _sa2->first); 
		if(*_sa1->first < *_sa2->first){
			return true;
		}else if(!(*_sa2->first < *_sa1->first)){
			return _sa1->second < _sa2->second;
		}
		return false;
	}
};

struct Inet6AddrPtrCmp{
	bool operator()(const Inet6SockAddrPair*const &_sa1, const Inet6SockAddrPair*const &_sa2)const{
		//TODO: optimize
		assert(_sa1 && _sa2); 
		if(*_sa1 < *_sa2){
			return true;
		}else if(!(*_sa2 < *_sa1)){
			return _sa1->port() < _sa2->port();
		}
		return false;
	}
	typedef std::pair<const Inet6SockAddrPair*, int>	PairProcAddr;
	bool operator()(const PairProcAddr* const &_sa1, const PairProcAddr* const &_sa2)const{
		assert(_sa1 && _sa2); 
		assert(_sa1->first && _sa2->first); 
		if(*_sa1->first < *_sa2->first){
			return true;
		}else if(!(*_sa2->first < *_sa1->first)){
			return _sa1->second < _sa2->second;
		}
		return false;
	}
};

typedef serialization::bin::RTTIMapper BinMapper;

struct Buffer{
	struct Header{
		uint8		version;
		uint8		type;
		uint16		flags;
		uint32		id;
		uint16		retransid;
		uint16		updatescnt;//16B
		const uint32& update(uint32 _pos)const;//the buffers that were received by peer
		uint32& update(uint32 _pos);//the buffers that were received by peer
		void pushUpdate(uint32 _upd);
		uint32 size()const{return sizeof(Header) + updatescnt * sizeof(uint32);}
	};
	enum Types{
		DataType = 1,
		ConnectingType,
		AcceptingType,
		Unknown
	};
	enum Flags{
		RequestReceipt = 1,//the buffer received update is sent imediately
		SwitchToNew = 1 << 1,
		SwitchToOld = 1 << 2,
		Connecting = 1 << 30,
		Accepted = 1 << 31	
	};
	Buffer(char *_pb = NULL, uint16 _bc = 0, uint16 _dl = 0):pb(_pb), bc(_bc), dl(_dl){
		if(_pb){
			reset(_dl);
		}
	}
	void reset(int _dl = 0){
		header().version = 1;
		header().retransid = 0;
		header().id = 0;
		header().flags = 0;
		header().type = Unknown;
		header().updatescnt = 0;
		dl = _dl;
	}
	void reinit(char *_pb = NULL, uint16 _bc = 0, uint16 _dl = 0){
		pb = _pb;
		bc = _bc;
		if(pb) reset(_dl);
	}
	char *buffer()const{return pb;}
	char *data()const {return pb + header().size();}
	
	uint32 bufferSize()const{return dl + header().size();}
	void bufferSize(uint32 _sz){
		assert(_sz >= header().size());
		dl = _sz - header().size();
	}
	uint32 bufferCapacity()const{return bc;}
	
	uint32 dataSize()const{return dl;}
	void dataSize(uint32 _dl){dl = _dl;}
	uint32 dataCapacity()const{return bc - header().size();}
	
	char* dataEnd()const{return data() + dataSize();}
	uint32 dataFreeSize()const{return dataCapacity() - dataSize();}
	
	char *release(uint32 &_cp){
		char* tmp = pb; pb = NULL; 
		_cp = bc; bc = 0; dl = 0;
		return tmp;
	}
	
	uint8 version()const{return header().version;}
	void version(uint8 _v){header().version = _v;}
	
	uint16 retransmitId()const{return header().retransid;} 
	void retransmitId(uint16 _ri){header().retransid = _ri;}
	
	uint32 id()const {return header().id;}
	void id(uint32 _id){header().id = _id;}
	
	uint16 flags()const {return header().flags;}
	void flags(uint16 _flags){header().flags = _flags;}
	
	uint8 type()const{return header().type;}
	void type(uint8 _tp){header().type = _tp;}
	
	uint32 updatesCount()const{return header().updatescnt;}
	uint32 update(uint32 _pos)const {return header().update(_pos);}
	void pushUpdate(uint32 _upd){header().pushUpdate(_upd);}
	
	Header& header(){return *reinterpret_cast<Header*>(pb);}
	const Header& header()const{return *reinterpret_cast<Header*>(pb);}
	
	bool check()const;
	
	void print()const;//see ipcservice.cpp
	
//data
	char	*pb;
	uint16	bc;//buffer capacity
	uint16	dl;//data length
};

//inlines:
inline const uint32& Buffer::Header::update(uint32 _pos)const{
	return *(reinterpret_cast<uint32*>(((char*)this) + sizeof(Header)) + _pos);
}
inline uint32& Buffer::Header::update(uint32 _pos){
	return *(reinterpret_cast<uint32*>(((char*)this) + sizeof(Header)) + _pos);
}
inline void Buffer::Header::pushUpdate(uint32 _upd){
	update(updatescnt++) = _upd;
}

}//namespace ipc
}//namespace clientserver

#endif
