/* Declarations file iodata.hpp
	
	Copyright 2007, 2008 Valentin Palade 
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

#ifndef FOUNDATION_IPC_SRC_IPC_IODATA_HPP
#define FOUNDATION_IPC_SRC_IPC_IODATA_HPP

#include "system/cassert.hpp"
#include "system/socketaddress.hpp"

namespace foundation{
class Visitor;
}

namespace foundation{
namespace ipc{

//*******	AddrPtrCmp		******************************************************************

struct Inet4AddrPtrCmp{
	
	bool operator()(const Inet4SockAddrPair*const &_sa1, const Inet4SockAddrPair*const &_sa2)const{
		//TODO: optimize
		cassert(_sa1 && _sa2); 
		if(*_sa1 < *_sa2){
			return true;
		}else if(!(*_sa2 < *_sa1)){
			return _sa1->port() < _sa2->port();
		}
		return false;
	}
	
	typedef std::pair<const Inet4SockAddrPair*, int>	PairProcAddr;
	
	bool operator()(const PairProcAddr* const &_sa1, const PairProcAddr* const &_sa2)const{
		cassert(_sa1 && _sa2); 
		cassert(_sa1->first && _sa2->first); 
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
		cassert(_sa1 && _sa2); 
		if(*_sa1 < *_sa2){
			return true;
		}else if(!(*_sa2 < *_sa1)){
			return _sa1->port() < _sa2->port();
		}
		return false;
	}
	
	typedef std::pair<const Inet6SockAddrPair*, int>	PairProcAddr;
	
	bool operator()(const PairProcAddr* const &_sa1, const PairProcAddr* const &_sa2)const{
		cassert(_sa1 && _sa2); 
		cassert(_sa1->first && _sa2->first); 
		if(*_sa1->first < *_sa2->first){
			return true;
		}else if(!(*_sa2->first < *_sa1->first)){
			return _sa1->second < _sa2->second;
		}
		return false;
	}
};

struct Buffer{
	enum{
		ReadCapacity = 4096,
		LastBufferId = 0xffffffff - 32,
		UpdateBufferId = 0xffffffff,//the id of a buffer containing only updates
	};
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
		KeepAliveType = 1,
		DataType,
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
	enum DataTypes{
		ContinuedSignal = 0,
		NewSignal,
		OldSignal
	};
	static uint32 minSize(){
		return sizeof(Header);
	}
	static char* allocateDataForReading();
	static void deallocateDataForReading(char *_buf);
	static const uint capacityForReading(){
		return ReadCapacity;
	}
	
	Buffer(
		char *_pb = NULL,
		uint16 _bc = 0,
		uint16 _dl = 0
	):bc(_bc), dl(_dl), pb(_pb){
	}
	Buffer(const Buffer& _rbuf):bc(_rbuf.bc), dl(_rbuf.dl), pb(_rbuf.release()){
	}
	Buffer& operator=(const Buffer& _rbuf){
		if(this != &_rbuf){
			bc = _rbuf.bc;
			dl = _rbuf.dl;
			pb = _rbuf.release();
		}
		return *this;
	}
	~Buffer();
	bool empty()const{
		return pb == NULL;
	}
	void resetHeader(){
		header().version = 1;
		header().retransid = 0;
		header().id = 0;
		header().flags = 0;
		header().type = Unknown;
		header().updatescnt = 0;
	}
	void clear();
	void optimize(uint16 _cp = 0);
	void reinit(char *_pb = NULL, uint16 _bc = 0, uint16 _dl = 0){
		clear();
		pb = _pb;
		bc = _bc;
		dl = _dl;
	}
	char *buffer()const{return pb;}
	char *data()const {return pb + header().size();}
	
	uint32 bufferSize()const{return dl + header().size();}
	void bufferSize(uint32 _sz){
		cassert(_sz >= header().size());
		dl = _sz - header().size();
	}
	uint32 bufferCapacity()const{return bc;}
	
	uint32 dataSize()const{return dl;}
	void dataSize(uint32 _dl){dl = _dl;}
	uint32 dataCapacity()const{return bc - header().size();}
	
	char* dataEnd()const{return data() + dataSize();}
	uint32 dataFreeSize()const{return dataCapacity() - dataSize();}
	
	void dataType(DataTypes _dt){
		uint8 dt = _dt;
		*reinterpret_cast<uint8*>(dataEnd()) = dt;
		++dl;
	}
	
	char *release(uint32 &_cp)const{
		char* tmp = pb; pb = NULL; 
		_cp = bc; bc = 0; dl = 0;
		return tmp;
	}
	char *release()const{
		uint32 cp;
		return release(cp);
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
	
public:
//data
	mutable uint16	bc;//buffer capacity
	mutable uint16	dl;//data length
	mutable char	*pb;
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


std::ostream& operator<<(std::ostream &_ros, const Buffer &_rb);
}//namespace ipc
}//namespace foundation

#endif
