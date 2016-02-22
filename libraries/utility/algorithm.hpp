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

//

inline bool compute_value_with_crc(uint64 &_to, uint64 _from){
	if(_from < (1ULL << 58)){
		_to = bit_count(_from) | (_from << 6);
		return true;
	}else{
		return false;
	}
}

inline bool check_value_with_crc(uint64 &_to, uint64 _v){
	_to = _v >> 6;
	if(bit_count(_to) == (_v & ((1 << 6) - 1))){
		return true;
	}else{
		return false;
	}
}

inline uint64 max_value_without_crc_64(){
	return (1ULL << 58) - 1ULL;
}

//

inline bool compute_value_with_crc(uint32 &_to, uint32 _from){
	if(_from < (1 << 27)){
		_to = bit_count(_from) | (_from << 5);
		return true;
	}else{
		return false;
	}
}

inline bool check_value_with_crc(uint32 &_to, uint32 _v){
	_to = _v >> 5;
	if(bit_count(_to) == (_v & ((1 << 5) - 1))){
		return true;
	}else{
		return false;
	}
}

inline uint32 max_value_without_crc_32(){
	return (1UL << 27) - 1UL;
}

//

inline bool compute_value_with_crc(uint16 &_to, uint16 _from){
	if(_from < (1 << 12)){
		_to = bit_count(_from) | (_from << 4);
		return true;
	}else{
		return false;
	}
}

inline bool check_value_with_crc(uint16 &_to, uint16 _v){
	_to = _v >> 4;
	if(bit_count(_to) == (_v & ((1 << 4) - 1))){
		return true;
	}else{
		return false;
	}
}

inline uint16 max_value_without_crc_16(){
	return ((1 << 12) - 1);
}

//

inline bool compute_value_with_crc(uint8 &_to, uint8 _from){
	if(_from < (1 << 5)){
		_to = bit_count(_from) | (_from << 3);
		return true;
	}else{
		return false;
	}
}

inline bool check_value_with_crc(uint8 &_to, uint8 _v){
	_to = _v >> 3;
	if(bit_count(_to) == (_v & ((1 << 3) - 1))){
		return true;
	}else{
		return false;
	}
}

inline uint8 max_value_without_crc_8(){
	return ((1 << 5) - 1);
}

inline size_t max_bit_count(uint8 _v){
	return 8 - leading_zero_count(_v);
}

inline size_t max_padded_bit_cout(uint8 _v){
	return fast_padded_size(max_bit_count(_v), 3);
}

inline size_t max_padded_byte_cout(uint8 _v){
	return max_padded_bit_cout(_v) >> 3;
}

//---
inline size_t max_bit_count(uint16 _v){
	return 16 - leading_zero_count(_v);
}

inline size_t max_padded_bit_cout(uint16 _v){
	return fast_padded_size(max_bit_count(_v), 3);
}

inline size_t max_padded_byte_cout(uint16 _v){
	return max_padded_bit_cout(_v) >> 3;
}

//---
inline size_t max_bit_count(uint32 _v){
	return 32 - leading_zero_count(_v);
}

inline size_t max_padded_bit_cout(uint32 _v){
	return fast_padded_size(max_bit_count(_v), 3);
}

inline size_t max_padded_byte_cout(uint32 _v){
	return max_padded_bit_cout(_v) >> 3;
}

//---
inline size_t max_bit_count(uint64 _v){
	return 64 - leading_zero_count(_v);
}

inline size_t max_padded_bit_cout(uint64 _v){
	return fast_padded_size(max_bit_count(_v), 3);
}

inline size_t max_padded_byte_cout(uint64 _v){
	return max_padded_bit_cout(_v) >> 3;
}
//---

//=============================================================================
#if 0
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
#endif
//=============================================================================

}//namespace solid


#endif
