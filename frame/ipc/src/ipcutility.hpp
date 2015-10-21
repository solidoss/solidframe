// frame/ipc/src/ipcutility.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_SRC_IPC_UTILITY_HPP
#define SOLID_FRAME_IPC_SRC_IPC_UTILITY_HPP

#include "system/cassert.hpp"
#include "system/socketaddress.hpp"

#include "frame/ipc/ipcservice.hpp"

namespace solid{
namespace frame{
namespace ipc{

enum{
	MinimumFreePacketDataSize = 16,
	MaxPacketDataSize = 1024 * 64,
};


struct SocketAddressHash{
	size_t operator()(const SocketAddressInet* const &_rsa)const{
		return _rsa->hash();
	}
};

struct SocketAddressEqual{
	bool operator()(
		const SocketAddressInet* const &_rsa1,
		const SocketAddressInet* const &_rsa2
	)const{
		return *_rsa1 == *_rsa2;
	}
};

struct PacketHeader{
	enum {
        SizeOfE = 4,
    };
	
	enum Types{
		SwitchToNewMessageTypeE = 1,
		SwitchToOldMessageTypeE,
		ContinuedMessageTypeE,
		
		KeepAliveTypeE,
		
	};
	
	enum Flags{
		Size64KBFlagE = 1, // DO NOT CHANGE!!
		CompressedFlagE = 2,
	};
	
	PacketHeader(
		const uint8 _type = 0,
		const uint8 _flags = 0,
		const uint16 _size = 0
	){
		reset(_type, _flags, _size);
	}
	
	void reset(
		const uint8 _type = 0,
		const uint8 _flags = 0,
		const uint16 _size = 0
	){
		type(_type);
		flags(_flags);
		size(_size);
	}
	
	uint32 size()const{
		uint32 sz = (m_flags & Size64KBFlagE);
		return (sz << 16) | m_size;
	}
	
	
	uint8 type()const{
		return m_type;
	}
	
	uint8 flags()const{
		return m_flags;
	}
	
	void type(uint8 _type){
		m_type = _type;
	}
	void flags(uint8 _flags){
		m_flags = _flags /*& (0xff - Size64KBFlagE)*/;
	}
	
	void size(uint32 _sz){
		m_size = _sz & 0xffff;
		m_flags |= ((_sz & (1 << 16)) >> 16);
	}
	
	bool isTypeKeepAlive()const{
		return m_type == KeepAliveTypeE;
	}
	
    bool isCompressed()const{
        return m_flags & CompressedFlagE;
    }
    bool isOk()const{
		bool rv = true;
		switch(m_type){
			case SwitchToNewMessageTypeE:
			case SwitchToOldMessageTypeE:
			case ContinuedMessageTypeE:
			case KeepAliveTypeE:
				break;
			default:
				rv = false;
				break;
		}
		
		if(size() > MaxPacketDataSize){
			rv = false;
		}
		
		return rv;
	}
    
    template <class S>
    char* store(char * _pc)const{
		_pc = S::storeValue(_pc, m_type);
		_pc = S::storeValue(_pc, m_flags);
		_pc = S::storeValue(_pc, m_size);
		return _pc;
	}
	
	template <class D>
    const char* load(const char *_pc){
		_pc = D::loadValue(_pc, m_type);
		_pc = D::loadValue(_pc, m_flags);
		_pc = D::loadValue(_pc, m_size);
		return _pc;
	}
private:
	uint8	m_type;
	uint8	m_flags;
	uint16	m_size;
};

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
