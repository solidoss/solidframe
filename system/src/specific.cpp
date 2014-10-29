// system/src/specific.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <stack>
#include <vector>
#include "system/common.hpp"
#include "system/cassert.hpp"
#include "system/specific.hpp"
#include "system/thread.hpp"
#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/exception.hpp"

namespace solid{


#ifdef HAS_SAFE_STATIC
static const unsigned specificPosition(){
	static const unsigned	thrspecpos = Thread::specificId();
	return thrspecpos;
}
#else
const unsigned specificPositionStub(){
	static const unsigned	thrspecpos = Thread::specificId();
	return thrspecpos;
}

void once_cbk(){
	specificPositionStub();
}

const unsigned specificPosition(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_cbk, once);
	return specificPositionStub();
}

#endif

//----------------------------------------------------------------------------------------------------
/*static*/ void Specific::destroy(void *_pv){
	Specific *ps = reinterpret_cast<Specific*>(_pv);
	delete ps;
}

/*static*/ Specific& Specific::prepareThread(
	const size_t _pagecp,
	const size_t _emptypagecnt
){
	Specific *ps = static_cast<Specific*>(Thread::specific(specificPosition()));
	if(ps == NULL){
		ps = new Specific(_pagecp, _emptypagecnt);
		Thread::specific(specificPosition(), ps, Specific::destroy);
	}
	return *ps;
}

// /*static*/ Specific& Specific::the(){
// 	Specific *pspec = static_cast<Specific*>(Thread::specific(specificPosition()));
// 	if(pspec){
// 		return *pspec;
// 	}else{
// 		return prepareThread();
// 	}
// 	
// }


}//namespace solid
