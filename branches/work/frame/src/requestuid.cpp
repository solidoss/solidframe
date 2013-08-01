// frame/src/requestuid.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/requestuid.hpp"
#include "system/thread.hpp"

namespace solid{
namespace frame{

struct SpecificRequestUid::ForcedCreate{
};

SpecificRequestUid requestuidptr;//(SpecificRequestUid::ForcedCreate);

#ifdef HAS_SAFE_STATIC
static const unsigned specificPosition(){
	static const unsigned	thrspecpos = Thread::specificId();
	return thrspecpos;
}
#else
const unsigned specificIdStub(){
	static const uint id(Thread::specificId());
	return id;
}
void once_stub(){
	specificIdStub();
}

const unsigned specificPosition(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_stub, once);
	return specificIdStub();
}
#endif
static void destroy(void *_pv){
	RequestUid *psd = reinterpret_cast<RequestUid*>(_pv);
	delete psd;
}

void SpecificRequestUid::prepareThread(){
	Thread::specific(specificPosition(), new RequestUid, &destroy);
}

SpecificRequestUid::SpecificRequestUid(const ForcedCreate&){
}

RequestUid* SpecificRequestUid::operator->()const{
	return reinterpret_cast<RequestUid*>(Thread::specific(specificPosition()));
}

RequestUid* SpecificRequestUid::get() const{
	return reinterpret_cast<RequestUid*>(Thread::specific(specificPosition()));
}

RequestUid& SpecificRequestUid::operator*()const{
	return *get();
}

}//namespace frame
}//namespace solid


