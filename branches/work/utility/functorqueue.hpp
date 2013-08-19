// utility/functorqueue.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_FUNCTORQUEUE_HPP
#define UTILITY_FUNCTORQUEUE_HPP

#include "system/common.hpp"

namespace solid{
template <
	class R = void,
	class P1 = EmptyType, class P2 = EmptyType, class P3 = EmptyType, class P4 = EmptyType, class P5 = EmptyType, class P6 = EmptyType, class P7 = EmptyType
>
class FunctorQueue;

template <
	typename R,
	typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7
>
class FunctorQueue{
public:
	FunctorQueue(){
	}
	
	~FunctorQueue(){
		clear();
	}
	R callFront(){
		return R();
	}
	
	R callFrontCopy(){}
	
	template <class T>
	void push(const T &_r){
		
	}
	
	void pop(){
	}
	
	void clear(){
		
	}
	
	size_t size()const{
		return 0;
	}
	bool empty()const{
		return true;
	}
	
};

}//namespace solid

#endif