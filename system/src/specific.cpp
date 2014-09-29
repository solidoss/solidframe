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

//This is what is held on a thread
struct Specific::Data{
	Data();
	~Data();
	
};

//----------------------------------------------------------------------------------------------------
void SpecificObject::operator delete (void *_pv, std::size_t _sz){
	Specific::the().free(_pv, _sz);
}
void* SpecificObject::operator new (std::size_t _sz){
	return Specific::the().allocate(_sz);
}
//----------------------------------------------------------------------------------------------------
//for caching objects
/*static*/ size_t Specific::stackid(Specific::CleanupFncT _pf){
	//The lock is not actually necessary - 
	Locker<Mutex>	lock(Thread::gmutex());
}

void Specific::doPushObject(const size_t _id, void *_pv){
}

void* Specific::doPopObject(const size_t _id){
}

}//namespace solid
