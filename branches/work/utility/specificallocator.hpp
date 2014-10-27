// utility/specificallocator.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_SPECIFIC_ALLOCATOR_HPP
#define UTILITY_SPECIFIC_ALLOCATOR_HPP

#include "system/specific.hpp"
#include <limits>

namespace solid{

template <class T>
class SpecificAllocator {
	public:
	// type definitions
	typedef T				value_type;
	typedef T*				pointer;
	typedef const T*		const_pointer;
	typedef T&				reference;
	typedef const T&		const_reference;
	typedef std::size_t		size_type;
	typedef std::ptrdiff_t	difference_type;

	// rebind allocator to type U
	template <class U>
	struct rebind {
		typedef SpecificAllocator<U> other;
	};

	// return address of values
	pointer address (reference value) const {
		return &value;
	}
	const_pointer address (const_reference value) const {
		return &value;
	}

	/* constructors and destructor
	* - nothing to do because the allocator has no state
	*/
	SpecificAllocator() throw() {
	}
	SpecificAllocator(const SpecificAllocator&) throw() {
	}
	template <class U>
	SpecificAllocator (const SpecificAllocator<U>&) throw() {
	}
	~SpecificAllocator() throw() {
	}

	// return maximum number of elements that can be allocated
	size_type max_size () const throw() {
		return std::numeric_limits<std::size_t>::max() / sizeof(T);
	}

	// allocate but don't initialize num elements of type T
	pointer allocate (size_type num, const void* = 0) {
		// print message and allocate memory with global new
		pointer ret = (pointer)(Specific::the().allocate(num*sizeof(T)));
		return ret;
	}

	// initialize elements of allocated storage p with value value
	void construct (pointer p, const T& value) {
		// initialize memory with placement new
		new((void*)p)T(value);
	}

	// destroy elements of initialized storage p
	void destroy (pointer p) {
		// destroy objects by calling their destructor
		p->~T();
	}

	// deallocate storage p of deleted elements
	void deallocate (pointer p, size_type num) {
		// print message and deallocate memory with global delete
		Specific::the().free(p, num * sizeof(T));
	}
};

// return that all specializations of this allocator are interchangeable
template <class T1, class T2>
bool operator== (
	const SpecificAllocator<T1>&,
	const SpecificAllocator<T2>&
) throw() {
	return true;
}

template <class T1, class T2>
bool operator!= (
	const SpecificAllocator<T1>&,
	const SpecificAllocator<T2>&
) throw() {
	return false;
}


}//namespace solid

#endif

