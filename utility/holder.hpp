// utility/holder.hpp
//
// Copyright (c) 2010 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UITLITY_HOLDER_HPP
#define UITLITY_HOLDER_HPP

#include "system/common.hpp"
#include "system/cassert.hpp"


namespace solid{

namespace impl{

typedef void(*DeleteF)(void *);

}//namespace impl

//! A template holder of objects
/*!
*/
template <size_t Cp = 32>
struct Holder{
	enum{
		Capacity = Cp > sizeof(void*) ? Cp : sizeof(void*)
	};
	
	
	//! Default constructor
	Holder(){
	}
	
	//! A template constructor for object's copy constructor
	template <class C>
	Holder(const C& _c){
	}
	
	//! Destructor
	/*!
		Only call the objects destructor
	*/
	~Holder(){
		
	}
	
	
	template <class C>
	C* pcast(){
		return reinterpret_cast<C*>(b);
	}
	
	template <class C>
	const C* pcast()const{
		return reinterpret_cast<const C*>(b);
	}
	
	template <class C>
	C& cast(){
		return *pcast();
	}
	
	template <class C>
	C const& cast()const{
		return *pcast();
	}
	
private:
	impl::DeleteF	pf;
	char			b[Capacity];
};

}//namespace solid

#endif
