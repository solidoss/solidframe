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

bool compute_value_with_crc(uint64 &_to, uint64 _from);
bool check_value_with_crc(uint64 &_to, uint64 _v);
inline uint64 max_value_without_crc_64(){
	return (1ULL << 58) - 1ULL;
}

bool compute_value_with_crc(uint32 &_to, uint32 _from);
bool check_value_with_crc(uint32 &_to, uint32 _v);
inline uint32 max_value_without_crc_32(){
	return (1UL << 27) - 1UL;
}

bool compute_value_with_crc(uint16 &_to, uint16 _from);
bool check_value_with_crc(uint16 &_to, uint16 _v);
inline uint16 max_value_without_crc_16(){
	return ((1 << 12) - 1);
}

bool compute_value_with_crc(uint8 &_to, uint8 _from);
bool check_value_with_crc(uint8 &_to, uint8 _v);
inline uint8 max_value_without_crc_8(){
	return ((1 << 5) - 1);
}

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
