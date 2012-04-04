/* Implementation file utility.cpp
	
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

#include "utility/workpool.hpp"
#include "utility/polycontainer.hpp"

uint8 bit_count(const uint8 _v){
	static const uint8 cnts[] = {
		0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
		4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
	};
	return cnts[_v];
}

uint16 bit_count(const uint16 _v){
	return bit_count((uint8)(_v & 0xff)) + bit_count((uint8)(_v >> 8));
}

uint32 bit_count(const uint32 _v){
	return bit_count((uint8)(_v & 0xff)) +
		bit_count((uint8)((_v >> 8) & 0xff)) +
		bit_count((uint8)((_v >> 16) & 0xff)) +
		bit_count((uint8)((_v >> 24) & 0xff));
}

uint64 bit_count(const uint64 _v){
	return bit_count((uint32)(_v & 0xffffffff)) +
		bit_count((uint32)(_v >> 32));
}

inline uint8 compute_crc_value(uint8 _pos){
	if(_pos < (1 << 5)){
		return (bit_count(_pos) << 5) | _pos;
	}else{
		return 0xff;
	}
}

inline uint16 compute_crc_value(uint16 _pos){
	if(_pos < (1 << 12)){
		return (bit_count(_pos) << 12) | _pos;
	}else{
		return 0xffff;
	}
}

inline uint32 compute_crc_value(uint32 _pos){
	if(_pos < (1 << 27)){
		return (bit_count(_pos) << 27) | _pos;
	}else{
		return 0xffffffff;
	}
}

inline uint64 compute_crc_value(uint64 _pos){
	if(_pos < (1ULL << 58)){
		return (bit_count(_pos) << 58) | _pos;
	}else{
		return -1LL;
	}
}


/*static*/ CRCValue<uint8> CRCValue<uint8>::check_and_create(uint8 _v){
	CRCValue<uint8> crcv(_v, true);
	if(crcv.crc() == bit_count(crcv.value())){
		return crcv;
	}else{
		return CRCValue<uint8>(0xff, true);
	}
}
/*static*/ bool CRCValue<uint8>::check(uint8 _v){
	CRCValue<uint8> crcv(_v, true);
	return crcv.crc() == bit_count(crcv.value());
}
CRCValue<uint8>::CRCValue(uint8 _v):v(compute_crc_value(_v)){
}

/*static*/ CRCValue<uint16> CRCValue<uint16>::check_and_create(uint16 _v){
	CRCValue<uint16> crcv(_v, true);
	if(crcv.crc() == bit_count(crcv.value())){
		return crcv;
	}else{
		return CRCValue<uint16>(0xffff, true);
	}
}
/*static*/ bool CRCValue<uint16>::check(uint16 _v){
	CRCValue<uint16> crcv(_v, true);
	return crcv.crc() == bit_count(crcv.value());
}
CRCValue<uint16>::CRCValue(uint16 _v):v(compute_crc_value(_v)){
}


/*static*/ CRCValue<uint32> CRCValue<uint32>::check_and_create(uint32 _v){
	CRCValue<uint32> crcv(_v, true);
	if(crcv.crc() == bit_count(crcv.value())){
		return crcv;
	}else{
		return CRCValue<uint32>(0xffffffff, true);
	}
}
/*static*/ bool CRCValue<uint32>::check(uint32 _v){
	CRCValue<uint32> crcv(_v, true);
	return crcv.crc() == bit_count(crcv.value());
}

CRCValue<uint32>::CRCValue(uint32 _v):v(compute_crc_value(_v)){
}

/*static*/ CRCValue<uint64> CRCValue<uint64>::check_and_create(uint64 _v){
	CRCValue<uint64> crcv(_v, true);
	if(crcv.crc() == bit_count(crcv.value())){
		return crcv;
	}else{
		return CRCValue<uint64>(-1LL, true);
	}
}
/*static*/ bool CRCValue<uint64>::check(uint64 _v){
	CRCValue<uint64> crcv(_v, true);
	return crcv.crc() == bit_count(crcv.value());
}

CRCValue<uint64>::CRCValue(uint64 _v):v(compute_crc_value(_v)){
}
