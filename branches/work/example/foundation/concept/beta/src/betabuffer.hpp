/* Declarations file betabuffer.hpp
	
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


#ifndef BETABUFFER_HPP
#define BETABUFFER_HPP

#include "system/common.hpp"

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
