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

//! A template holder of objects derived from a base class
/*!
	The interface resembles somehow that of a smart/auto pointer, but while
	a smart/auto pointer only holds a pointer/reference to an object
	a holder also keeps the object's allocation space.
	It uses new(buf) for allocating the object and calls the 
	destructor on deletion.
	For a good example of usage and benefits see ::protocol::Buffer,
	::protocol::Writer, ::protocol::Reader
*/
template <class B, uint32 Sz = sizeof(B)>
struct Holder{
	enum{
		Capacity = Sz > sizeof(B) ? Sz : sizeof(B)
	};
	//! Default constructor
	Holder(){
		new(b) B;
	}
	//! A template constructor for object's copy constructor
	template <class C>
	Holder(const C& _c){
		cstatic_assert(sizeof(C) <= Capacity);
		cassert(&static_cast<const B&>(_c) == &_c);
		new(b) C(_c);
	}
	//! Destructor
	/*!
		Only call the objects destructor
	*/
	~Holder(){
		get()->~B();
	}
	
	//! A template assertion operator
	template <class C>
	Holder& operator=(const C &_c){
		get()->~B();
		cstatic_assert(sizeof(C) <= Capacity);
		cassert(&static_cast<const B&>(_c) == &_c);
		new(b) C(_c);
		return *this;
	}
	//! Gets a reference to the internal object
	B& operator*(){
		return *get();
	}
	
	//! Gets a const reference to the internal object
	const B& operator*()const{
		return *get();
	}
	
	B* get()const{
		return reinterpret_cast<B*>(const_cast<char*>(b));
	}
	
	//! The interface for a pointer
	B* operator->()const{
		return get();
	}
private:
	char	b[Capacity];
};

}//namespace solid

#endif
