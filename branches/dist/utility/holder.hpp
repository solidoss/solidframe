/* Declarations file holder.hpp
	
	Copyright 2010 Valentin Palade 
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

#ifndef UITLITY_HOLDER_HPP
#define UITLITY_HOLDER_HPP

#include "system/common.hpp"
#include "system/cassert.hpp"

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
		ptr()->~B();
	}
	
	//! A template assertion operator
	template <class C>
	Holder& operator=(const C &_c){
		ptr()->~B();
		cstatic_assert(sizeof(C) <= Capacity);
		cassert(&static_cast<const B&>(_c) == &_c);
		new(b) C(_c);
		return *this;
	}
	//! Gets a reference to the internal object
	B& operator*(){
		return *ptr();
	}
	
	//! Gets a const reference to the internal object
	const B& operator*()const{
		return *ptr();
	}
	
	B* ptr()const{
		return reinterpret_cast<B*>(const_cast<char*>(b));
	}
	
	//! The interface for a pointer
	B* operator->()const{
		return ptr();
	}
private:
	char	b[Capacity];
};


#endif
