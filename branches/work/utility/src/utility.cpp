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
#include "utility/string.hpp"
#include "utility/polycontainer.hpp"
#include <cstring>
#ifndef ON_WINDOWS
#include <strings.h>
#endif


//------	PolyContainter --------------------------------------------

static const char* strs[] = {
 "Ox00", "Ox01", "Ox02", "Ox03", "Ox04", "Ox05", "Ox06", "Ox07", "Ox08",  "TAB",
   "LF", "Ox0B", "Ox0C",   "CR", "Ox0E", "Ox0F", "Ox10", "Ox11", "Ox12", "Ox13",
 "Ox14", "Ox15", "Ox16", "Ox17", "Ox18", "Ox19", "Ox1A", "Ox1B", "Ox1C", "Ox1D",
 "Ox1E", "Ox1F","SPACE",    "!",    "\"",    "#",    "$",    "%",    "&",    "'",
    "(",    ")",    "*",    "+",    ",",    "-",    ".",    "/",    "0",    "1",
    "2",    "3",    "4",    "5",    "6",    "7",    "8",    "9",    ":",    ";",
    "<",    "=",    ">",    "?",    "@",    "A",    "B",    "C",    "D",    "E",
    "F",    "G",    "H",    "I",    "J",    "K",    "L",    "M",    "N",    "O",
    "P",    "Q",    "R",    "S",    "T",    "U",    "V",    "W",    "X",    "Y",
    "Z",    "[",    "\\",    "]",    "^",    "_",    "`",    "a",    "b",    "c",
    "d",    "e",    "f",    "g",    "h",    "i",    "j",    "k",    "l",    "m",
    "n",    "o",    "p",    "q",    "r",    "s",    "t",    "u",    "v",    "w",
    "x",    "y",    "z",    "{",    "|",    "}",    "~", "Ox7F", "Ox80", "Ox81",
 "Ox82", "Ox83", "Ox84", "Ox85", "Ox86", "Ox87", "Ox88", "Ox89", "Ox8A", "Ox8B",
 "Ox8C", "Ox8D", "Ox8E", "Ox8F", "Ox90", "Ox91", "Ox92", "Ox93", "Ox94", "Ox95",
 "Ox96", "Ox97", "Ox98", "Ox99", "Ox9A", "Ox9B", "Ox9C", "Ox9D", "Ox9E", "Ox9F",
 "OxA0", "OxA1", "OxA2", "OxA3", "OxA4", "OxA5", "OxA6", "OxA7", "OxA8", "OxA9",
 "OxAA", "OxAB", "OxAC", "OxAD", "OxAE", "OxAF", "OxB0", "OxB1", "OxB2", "OxB3",
 "OxB4", "OxB5", "OxB6", "OxB7", "OxB8", "OxB9", "OxBA", "OxBB", "OxBC", "OxBD",
 "OxBE", "OxBF", "OxC0", "OxC1", "OxC2", "OxC3", "OxC4", "OxC5", "OxC6", "OxC7",
 "OxC8", "OxC9", "OxCA", "OxCB", "OxCC", "OxCD", "OxCE", "OxCF", "OxD0", "OxD1",
 "OxD2", "OxD3", "OxD4", "OxD5", "OxD6", "OxD7", "OxD8", "OxD9", "OxDA", "OxDB",
 "OxDC", "OxDD", "OxDE", "OxDF", "OxE0", "OxE1", "OxE2", "OxE3", "OxE4", "OxE5",
 "OxE6", "OxE7", "OxE8", "OxE9", "OxEA", "OxEB", "OxEC", "OxED", "OxEE", "OxEF",
 "OxF0", "OxF1", "OxF2", "OxF3", "OxF4", "OxF5", "OxF6", "OxF7", "OxF8", "OxF9",
 "OxFA", "OxFB", "OxFC", "OxFD", "OxFE", "OxFF"
};


const char * charToString(unsigned _c){
	return strs[_c & 255];
}


/*static*/ int cstring::cmp(const char* _s1, const char *_s2){
    return ::strcmp(_s1, _s2);
}

/*static*/ int cstring::ncmp(const char* _s1, const char *_s2, uint _len){
    return ::strncmp(_s1, _s2, _len);
}

/*static*/ int cstring::casecmp(const char* _s1, const char *_s2){
#ifdef ON_WINDOWS
        return _stricmp(_s1,_s2);
#else
        return ::strcasecmp(_s1,_s2);
#endif
}

/*static*/ int cstring::ncasecmp(const char* _s1, const char *_s2, uint _len){
#ifdef ON_WINDOWS
        return _strnicmp(_s1,_s2, _len);
#else
        return strncasecmp(_s1,_s2, _len);
#endif
}

/*static*/ size_t cstring::nlen(const char *s, size_t maxlen){
#ifdef ON_MACOS
    return strlen(s);
#else
	return strnlen(s, maxlen);
#endif
}

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
