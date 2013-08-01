// betabuffer.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef BETABUFFER_HPP
#define BETABUFFER_HPP

#include "system/common.hpp"

using solid::uint32;
using solid::uint8;
using solid::uint16;

namespace concept{

namespace beta{

struct BufferHeader{
	enum Types{
		Data = 1,
		Exception = 2
	};
	enum Flags{
		Compressed = 1,
		Encrypted = 2
	};
	enum{
		Size = sizeof(uint32)
	};
	BufferHeader(
		uint16 _size,
		const uint8 _type = Data,
		const uint8 _flags = 0
	){
		value = _type;
		value <<= 8;
		value |= _flags;
		value <<= 16;
		value |= _size;
	}
	BufferHeader(uint32 _value):value(_value){}
	BufferHeader():value(0){}
	uint8 type()const{
		return value >> 24;
	}
	uint8 flags()const{
		return (value >> 16) & 0xff;
	}
	uint16 size()const{
		return value & 0xffff;
	}
	bool isData()const{
		return type() == Data;
	}
	bool isException()const{
		return type() == Exception;
	}
	bool isCompressed()const{
		return flags() & Compressed;
	}
	bool isEncrypted()const{
		return flags() & Encrypted;
	}
	uint32	value;
};


}//namespace beta

}//namespace concept

#endif
