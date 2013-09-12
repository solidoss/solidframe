// utility/memory.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_MEMORY_HPP
#define UTILITY_MEMORY_HPP

#include "system/common.hpp"
#include "system/atomic.hpp"

namespace solid{

struct EmptyChecker{
	EmptyChecker(const char *_fncname):v(0), fncname(_fncname){}
	~EmptyChecker();
	void add();
	void sub();
private:
	typedef ATOMIC_NS::atomic<size_t>			AtomicSizeT;
	AtomicSizeT	v;
	const char 	*fncname;
};

#ifdef UDEBUG

#ifdef HAS_SAFE_STATIC
template <class T>
void objectCheck(bool _add, const char *_fncname){
	static EmptyChecker ec(_fncname);
	if(_add)	ec.add();
	else 		ec.sub();
}
#else
template <class T>
void objectCheckStub(bool _add, const char *_fncname){
	static EmptyChecker ec(_fncname);
	if(_add)	ec.add();
	else 		ec.sub();
}

template <class T>
void once_cbk_memory(){
	objectCheckStub<T>(true, "once_cbk_memory");
	objectCheckStub<T>(false, "once_cbk_memory");
}

template <class T>
void objectCheck(bool _add, const char *_fncname){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_cbk_memory<T>, once);
	return objectCheckStub<T>(_add, _fncname);
}


#endif

#else
template <class T>
void objectCheck(bool _add, const char *){
}
#endif

}//namespace solid


#endif
