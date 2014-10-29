// system/specific.hpp
//
// Copyright (c) 2007, 2008, 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_SPECIFIC_HPP
#define SYSTEM_SPECIFIC_HPP

#include <cstddef>
#include <new>

#include "system/common.hpp"
#include "system/memorycache.hpp"

namespace solid{

class Thread;
//! A base class for thread specific objects
/*!
	A thread specific object is allocated and destroyed by one and the same thread.
	It will try to reuse a buffer from thread's cache, and on delete (which must be
	called on the same thread as new), it will recache the data
*/
struct SpecificObject{
	static void operator delete (void *_p, std::size_t _sz);
	static void* operator new (std::size_t _sz);
	static void * operator new (std::size_t, void *_p) throw() {
		return _p;
	}
};

//! A thread specific container wrapper
/*!
	It can cache:<br>
	- objects given by pointers uncache/cache/tryUncache
	- buffers of size power of 2
*/
class Specific{
public:
	static Specific& the();
	
	void configure(
		const size_t _pagecp = 0,
		const size_t _emptypagecnt = 1
	);
	
	void *allocate(const size_t _cp);
	
	void free(void *_p, const size_t _cp);
	
	size_t reserve(const size_t _sz, const size_t _cnt, const bool _lazy = true);
	
private:
	friend class Thread;
	static void destroy(void *_pv);
	//! One cannot create a Specific object
	Specific(){}
	Specific(const Specific&);
	Specific& operator=(const Specific&);
private:
	MemoryCache	cache;
};

inline void *Specific::allocate(const size_t _cp){
	return cache.allocate(_cp);
}
	
inline void Specific::free(void *_p, const size_t _cp){
	return cache.free(_p, _cp);
}

inline size_t Specific::reserve(const size_t _sz, const size_t _cnt, const bool _lazy){
	return cache.reserve(_sz, _cnt, _lazy);
}


inline void SpecificObject::operator delete (void *_pv, std::size_t _sz){
	Specific::the().free(_pv, _sz);
}

inline void* SpecificObject::operator new (std::size_t _sz){
	return Specific::the().allocate(_sz);
}

}//namespace solid

#endif
