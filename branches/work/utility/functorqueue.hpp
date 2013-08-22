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
	typename R = void,
	typename P1 = void, typename P2 = void,
	typename P3 = void, typename P4 = void
>
class FunctorQueue;

template <
	typename R
>
class FunctorQueue<R, void, void, void, void>{
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