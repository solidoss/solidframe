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
		DataTypeE = 1,
		KeepAliveTypeE = 2,
	};
	enum Flags{
		CompressedFlagE = 1,
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
        return type == DataTypeE;
    }
    
    bool isCompressed()const{
        return flags & CompressedFlagE;
    }
    template <class S>
    char* store(char * _pc)const{
		_pc = S::storeValue(_pc, type);
		_pc = S::storeValue(_pc, flags);
		_pc = S::storeValue(_pc, size);
		return _pc;
	}
	
	template <class D>
    const char* load(const char *_pc){
		_pc = D::loadValue(_pc, type);
		_pc = D::loadValue(_pc, flags);
		_pc = D::loadValue(_pc, size);
		return _pc;
	}
	
	uint8	type;
	uint8	flags;
	uint16	size;
};

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
