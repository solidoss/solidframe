// solid/utility/algorithm.hpp
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

#include <utility>
#include "solid/utility/common.hpp"

namespace solid{

//

inline bool compute_value_with_crc(uint64_t &_to, uint64_t _from){
	if(_from < (1ULL << 58)){
		_to = bit_count(_from) | (_from << 6);
		return true;
	}else{
		return false;
	}
}

inline bool check_value_with_crc(uint64_t &_to, uint64_t _v){
	_to = _v >> 6;
	if(bit_count(_to) == (_v & ((1 << 6) - 1))){
		return true;
	}else{
		return false;
	}
}

inline uint64_t max_value_without_crc_64(){
	return (1ULL << 58) - 1ULL;
}

//

inline bool compute_value_with_crc(uint32_t &_to, uint32_t _from){
	if(_from < (1 << 27)){
		_to = bit_count(_from) | (_from << 5);
		return true;
	}else{
		return false;
	}
}

inline bool check_value_with_crc(uint32_t &_to, uint32_t _v){
	_to = _v >> 5;
	if(bit_count(_to) == (_v & ((1 << 5) - 1))){
		return true;
	}else{
		return false;
	}
}

inline uint32_t max_value_without_crc_32(){
	return (1UL << 27) - 1UL;
}

//

inline bool compute_value_with_crc(uint16_t &_to, uint16_t _from){
	if(_from < (1 << 12)){
		_to = bit_count(_from) | (_from << 4);
		return true;
	}else{
		return false;
	}
}

inline bool check_value_with_crc(uint16_t &_to, uint16_t _v){
	_to = _v >> 4;
	if(bit_count(_to) == (_v & ((1 << 4) - 1))){
		return true;
	}else{
		return false;
	}
}

inline uint16_t max_value_without_crc_16(){
	return ((1 << 12) - 1);
}

//

inline bool compute_value_with_crc(uint8_t &_to, uint8_t _from){
	if(_from < (1 << 5)){
		_to = bit_count(_from) | (_from << 3);
		return true;
	}else{
		return false;
	}
}

inline bool check_value_with_crc(uint8_t &_to, uint8_t _v){
	_to = _v >> 3;
	if(bit_count(_to) == (_v & ((1 << 3) - 1))){
		return true;
	}else{
		return false;
	}
}

inline uint8_t max_value_without_crc_8(){
	return ((1 << 5) - 1);
}

inline size_t max_bit_count(uint8_t _v){
	return 8 - leading_zero_count(_v);
}

inline size_t max_padded_bit_cout(uint8_t _v){
	return fast_padded_size(max_bit_count(_v), 3);
}

inline size_t max_padded_byte_cout(uint8_t _v){
	return max_padded_bit_cout(_v) >> 3;
}

//---
inline size_t max_bit_count(uint16_t _v){
	return 16 - leading_zero_count(_v);
}

inline size_t max_padded_bit_cout(uint16_t _v){
	return fast_padded_size(max_bit_count(_v), 3);
}

inline size_t max_padded_byte_cout(uint16_t _v){
	return max_padded_bit_cout(_v) >> 3;
}

//---
inline size_t max_bit_count(uint32_t _v){
	return 32 - leading_zero_count(_v);
}

inline size_t max_padded_bit_cout(uint32_t _v){
	return fast_padded_size(max_bit_count(_v), 3);
}

inline size_t max_padded_byte_cout(uint32_t _v){
	return max_padded_bit_cout(_v) >> 3;
}

//---
inline size_t max_bit_count(uint64_t _v){
	return 64 - leading_zero_count(_v);
}

inline size_t max_padded_bit_cout(uint64_t _v){
	return fast_padded_size(max_bit_count(_v), 3);
}

inline size_t max_padded_byte_cout(uint64_t _v){
	return max_padded_bit_cout(_v) >> 3;
}
//---

//=============================================================================
#if 1
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

struct binary_search_basic_comparator{
	template <typename K1, typename K2>
	int operator()(const K1 &_k1, const K2 &_k2)const{
		if(_k1 < _k2) return -1;
		if(_k2 < _k1) return 1;
		return 0;
	}
};

using binary_search_result_t =  std::pair<bool, size_t>;

template<class It, class Key, class Compare = binary_search_basic_comparator>
binary_search_result_t binary_search(It _from, It _to, const Key &_rk, const Compare &_rcmp = Compare()){
	const It	beg(_from);
	size_t		midpos;
	while(_to > _from){
		midpos = (_to - _from) >> 1;
		int r = _rcmp(*(_from + midpos), _rk);
		if(!r) return binary_search_result_t(true, _from - beg + midpos);
		if(r < 0){
			_from += (midpos + 1);
		}else{
			_to = _from + midpos;
		}
	}
	return binary_search_result_t(false, _from - beg);
}

template<class It, class Key, class Compare = binary_search_basic_comparator>
binary_search_result_t binary_search_first(It _from, It _to, const Key &_rk, const Compare &_rcmp = Compare()){
	binary_search_result_t p = solid::binary_search(_from, _to, _rk, _rcmp);
	
	if(!p.first) return p;//not found
	
	while(p.second && !_rcmp(*(_from + p.second - 1), _rk)){
		p =  solid::binary_search(_from, _from + p.second, _rk, _rcmp);
	}
	return p;
}
	
template<class It, class Key, class Compare = binary_search_basic_comparator>
binary_search_result_t binary_search_last(It _from, It _to, const Key &_rk, const Compare &_rcmp = Compare()){
	binary_search_result_t p = solid::binary_search(_from, _to, _rk, _rcmp);
	
	if(!p.first) return p;//not found
	
	while(p.second != (_to - _from - 1) && !_rcmp(*(_from + p.second + 1), _rk)){
		p =  solid::binary_search(_from + p.second + 1, _to, _rk, _rcmp);
	}
	return p;
}

}//namespace solid


#endif
