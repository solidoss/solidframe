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
#ifdef HAVE_CPP11

struct SockAddrHash{
	size_t operator()(const SocketAddressPair4*const &_psa)const{
		return _psa->addr->sin_addr.s_addr ^ _psa->addr->sin_port;
	}
	
	typedef std::pair<const SocketAddressPair4*, int>	PairProcAddr;
	
	size_t operator()(const PairProcAddr* const &_psa)const{
		return _psa->first->addr->sin_addr.s_addr ^ _psa->second;
	}
};

struct SockAddrEqual{
	bool operator()(const SocketAddressPair4*const &_psa1, const SocketAddressPair4*const &_psa2)const{
		return _psa1->addr->sin_addr.s_addr == _psa2->addr->sin_addr.s_addr &&
		_psa1->addr->sin_port == _psa2->addr->sin_port;
	}
	
	typedef std::pair<const SocketAddressPair4*, int>	PairProcAddr;
	
	bool operator()(const PairProcAddr* const &_psa1, const PairProcAddr* const &_psa2)const{
		return _psa1->first->addr->sin_addr.s_addr == _psa2->first->addr->sin_addr.s_addr &&
		_psa1->second == _psa2->second;
	}
};

#else

struct Inet4AddrPtrCmp{
	
	bool operator()(const SocketAddressPair4*const &_sa1, const SocketAddressPair4*const &_sa2)const{
		//TODO: optimize
		cassert(_sa1 && _sa2); 
		if(*_sa1 < *_sa2){
			return true;
		}else if(!(*_sa2 < *_sa1)){
			return _sa1->port() < _sa2->port();
		}
		return false;
	}
	
	typedef std::pair<const SocketAddressPair4*, int>	PairProcAddr;
	
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
	
	bool operator()(const SocketAddressPair6*const &_sa1, const SocketAddressPair6*const &_sa2)const{
		//TODO: optimize
		cassert(_sa1 && _sa2); 
		if(*_sa1 < *_sa2){
			return true;
		}else if(!(*_sa2 < *_sa1)){
			return _sa1->port() < _sa2->port();
		}
		return false;
	}
	
	typedef std::pair<const SocketAddressPair6*, int>	PairProcAddr;
	
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

#endif

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
		
		uint32 size()const;
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
	static uint32 minSize();
	static char* allocateDataForReading();
	static void deallocateDataForReading(char *_buf);
	static const uint capacityForReading();
	
	Buffer(
		char *_pb = NULL,
		uint16 _bc = 0,
		uint16 _dl = 0
	);
	Buffer(const Buffer& _rbuf);
	Buffer& operator=(const Buffer& _rbuf);
	~Buffer();
	bool empty()const;
	void resetHeader();
	void clear();
	void optimize(uint16 _cp = 0);
	void reinit(char *_pb = NULL, uint16 _bc = 0, uint16 _dl = 0);
	char *buffer()const;
	char *data()const ;
	
	uint32 bufferSize()const;
	void bufferSize(uint32 _sz);
	uint32 bufferCapacity()const;
	
	uint32 dataSize()const;
	void dataSize(uint32 _dl);
	uint32 dataCapacity()const;
	
	char* dataEnd()const;
	uint32 dataFreeSize()const;
	
	void dataType(DataTypes _dt);
	
	char *release(uint32 &_cp)const;
	char *release()const;
	
	uint8 version()const;
	void version(uint8 _v);
	
	uint16 retransmitId()const;
	void retransmitId(uint16 _ri);
	
	uint32 id()const;
	void id(uint32 _id);
	
	uint16 flags()const;
	void flags(uint16 _flags);
	
	uint8 type()const;
	void type(uint8 _tp);
	
	uint32 updatesCount()const;
	uint32 update(uint32 _pos)const;
	void pushUpdate(uint32 _upd);
	
	Header& header();
	const Header& header()const;
	
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
inline uint32 Buffer::Header::size()const{
	return sizeof(Header) + updatescnt * sizeof(uint32);
}

inline /*static*/ uint32 Buffer::minSize(){
		return sizeof(Header);
	}
inline /*static*/ const uint Buffer::capacityForReading(){
	return ReadCapacity;
}

inline Buffer::Buffer(
	char *_pb,
	uint16 _bc,
	uint16 _dl
):bc(_bc), dl(_dl), pb(_pb){
}

inline Buffer::Buffer(const Buffer& _rbuf):bc(_rbuf.bc), dl(_rbuf.dl), pb(_rbuf.release()){
}

inline Buffer& Buffer::operator=(const Buffer& _rbuf){
	if(this != &_rbuf){
		bc = _rbuf.bc;
		dl = _rbuf.dl;
		pb = _rbuf.release();
	}
	return *this;
}

inline bool Buffer::empty()const{
	return pb == NULL;
}

inline void Buffer::resetHeader(){
	header().version = 1;
	header().retransid = 0;
	header().id = 0;
	header().flags = 0;
	header().type = Unknown;
	header().updatescnt = 0;
}

inline void Buffer::reinit(char *_pb, uint16 _bc, uint16 _dl){
	clear();
	pb = _pb;
	bc = _bc;
	dl = _dl;
}

inline char *Buffer::buffer()const{
	return pb;
}

inline char *Buffer::data()const{
	return pb + header().size();
}

inline uint32 Buffer::bufferSize()const{
	return dl + header().size();
}

inline void Buffer::bufferSize(uint32 _sz){
	cassert(_sz >= header().size());
	dl = _sz - header().size();
}

inline uint32 Buffer::bufferCapacity()const{
	return bc;
}

inline uint32 Buffer::dataSize()const{
	return dl;
}

inline void Buffer::dataSize(uint32 _dl){
	dl = _dl;
}

inline uint32 Buffer::dataCapacity()const{
	return bc - header().size();
}

inline char* Buffer::dataEnd()const{
	return data() + dataSize();
}

inline uint32 Buffer::dataFreeSize()const{
	return dataCapacity() - dataSize();
}

inline void Buffer::dataType(DataTypes _dt){
	uint8 dt = _dt;
	*reinterpret_cast<uint8*>(dataEnd()) = dt;
	++dl;
}

inline char *Buffer::release(uint32 &_cp)const{
	char* tmp = pb; pb = NULL; 
	_cp = bc; bc = 0; dl = 0;
	return tmp;
}

inline char *Buffer::release()const{
	uint32 cp;
	return release(cp);
}

inline uint8 Buffer::version()const{
	return header().version;
}

inline void Buffer::version(uint8 _v){
	header().version = _v;
}

inline uint16 Buffer::retransmitId()const{
	return header().retransid;
} 

inline void Buffer::retransmitId(uint16 _ri){
	header().retransid = _ri;
}

inline uint32 Buffer::id()const {
	return header().id;
}

inline void Buffer::id(uint32 _id){
	header().id = _id;
}

inline uint16 Buffer::flags()const{
	return header().flags;
}

inline void Buffer::flags(uint16 _flags){
	header().flags = _flags;
}

inline uint8 Buffer::type()const{
	return header().type;
}

inline void Buffer::type(uint8 _tp){
	header().type = _tp;
}

inline uint32 Buffer::updatesCount()const{
	return header().updatescnt;
}

inline uint32 Buffer::update(uint32 _pos)const{
	return header().update(_pos);
}

inline void Buffer::pushUpdate(uint32 _upd){
	header().pushUpdate(_upd);
}

inline Buffer::Header& Buffer::header(){
	return *reinterpret_cast<Header*>(pb);
}

inline const Buffer::Header& Buffer::header()const{
	return *reinterpret_cast<Header*>(pb);
}


std::ostream& operator<<(std::ostream &_ros, const Buffer &_rb);
}//namespace ipc
}//namespace foundation

#endif
