// utility/algorithm.hpp
//
// Copyright (c) 20015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_ALGORITHM_HPP
#define UTILITY_ALGORITHM_HPP

#include "utility/common.hpp"

namespace solid{

//=============================================================================
template <typename T>
struct CRCValue;


template<>
struct CRCValue<uint64>{
	static CRCValue<uint64> check_and_create(uint64 _v);
	static bool check(uint64 _v);
	static const uint64 maximum(){
		return (1ULL << 58) - 1ULL;
	}
	
	CRCValue(uint64 _v);
	CRCValue(const CRCValue<uint64> &_v):v(_v.v){}
	
	bool ok()const{
		return v != (uint64)-1;
	}
	const uint64 value()const{
		return v >> 6;
	}
	
	const uint64 crc()const{
		return v & ((1ULL << 6) - 1);
	}
	
	operator uint64()const{
		return v;
	}
	
private:
	CRCValue(uint64 _v, bool):v(_v){}
	const uint64	v;
};


template<>
struct CRCValue<uint32>{
	static CRCValue<uint32> check_and_create(uint32 _v);
	static bool check(uint32 _v);
	static const uint32 maximum(){
		return (1UL << 27) - 1UL;
	}
	
	CRCValue(uint32 _v);
	CRCValue(const CRCValue<uint32> &_v):v(_v.v){}
	
	bool ok()const{
		return v != (uint32)-1;
	}
	uint32 value()const{
		//return v & ((1UL << 27) - 1);
		return v >> 5;
	}
	
	uint32 crc()const{
		//return v >> 27;
		return v & ((1UL << 5) - 1);
	}
	
	operator uint32()const{
		return v;
	}
	
private:
	CRCValue(uint32 _v, bool):v(_v){}
	const uint32	v;
};

template<>
struct CRCValue<uint16>{
	static CRCValue<uint16> check_and_create(uint16 _v);
	static bool check(uint16 _v);
	static const uint16 maximum(){
		return ((1 << 12) - 1);
	}
	
	CRCValue(uint16 _idx);
	CRCValue(const CRCValue<uint16> &_v):v(_v.v){}
	
	bool ok()const{
		return v != (uint16)-1;
	}
	uint16 value()const{
		//return v & ((1 << 12) - 1);
		return v >> 4;
	}
	uint16 crc()const{
		//return v >> 12;
		return v & ((1 << 4) - 1);
	}
	operator uint16()const{
		return v;
	}
private:
	CRCValue(uint16 _v, bool):v(_v){}
	const uint16	v;
};

template<>
struct CRCValue<uint8>{
	static CRCValue<uint8> check_and_create(uint8 _v);
	static bool check(uint8 _v);
	static const uint8 maximum(){
		return ((1 << 5) - 1);
	}
	
	CRCValue(uint8 _idx);
	CRCValue(const CRCValue<uint8> &_v):v(_v.v){}
	bool ok()const{
		return v != (uint8)-1;
	}
	uint8 value()const{
		//return v & ((1 << 5) - 1);
		return v >> 3;
	}
	uint8 crc()const{
		//return v >> 5;
		return v & ((1 << 3) - 1);
	}
	operator uint8()const{
		return v;
	}
private:
	CRCValue(uint8 _v, bool):v(_v){}
	const uint8	v;
};

//=============================================================================

template <class It, class Cmp>
size_t find_cmp(It _it, Cmp const &, SizeToType<1> _s){
	return 0;
}

template <class It, class Cmp>
size_t find_cmp(It _it, Cmp const &_rcmp, SizeToType<2> _s){
	if(_rcmp(*_it, *(_it + 1))){
		return 0;
	}
	return 1;
}

template <class It, class Cmp, size_t S>
size_t find_cmp(It _it, Cmp const &_rcmp, SizeToType<S> s){
	
	const size_t	off1 = find_cmp(_it, _rcmp, SizeToType<S/2>());
	const size_t	off2 = find_cmp(_it + S/2, _rcmp, SizeToType<S - S/2>()) + S/2;
	
	if(_rcmp(*(_it + off1), *(_it + off2))){
		return off1;
	}
	return off2;
}

//=============================================================================

}//namespace solid


#endif
