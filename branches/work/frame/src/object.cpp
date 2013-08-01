// frame/src/object.cpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "system/cassert.hpp"
#include "system/mutex.hpp"
#include "system/thread.hpp"
#include "system/debug.hpp"

#include "frame/object.hpp"
#include "frame/message.hpp"
#include "frame/manager.hpp"
#include "frame/service.hpp"

#include "utility/memory.hpp"
#include "utility/dynamicpointer.hpp"

namespace solid{
//--------------------------------------------------------------
namespace{
#ifdef HAS_SAFE_STATIC
static const unsigned specificPosition(){
	static const unsigned	thrspecpos = Thread::specificId();
	return thrspecpos;
}
static const uint currentTimeSpecificPosition(){
	static const uint id(Thread::specificId());
	return id;
}
#else
const uint specificIdStub(){
	static const uint id(Thread::specificId());
	return id;
}
const uint timeSpecificIdStub(){
	static const uint id(Thread::specificId());
	return id;
}

void once_stub(){
	specificIdStub();
}

void once_time_stub(){
	timeSpecificIdStub();
}

static const unsigned specificPosition(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_stub, once);
	return specificIdStub();
}
static const uint currentTimeSpecificPosition(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_time_stub, once);
	return timeSpecificIdStub();
}
#endif
}

namespace frame{
//---------------------------------------------------------------------
//----	Object	----
//---------------------------------------------------------------------

#ifdef NINLINES
#include "frame/object.ipp"
#endif

/*static*/ const TimeSpec& Object::currentTime(){
	return *reinterpret_cast<const TimeSpec*>(Thread::specific(currentTimeSpecificPosition()));
}
/*static*/ void Object::doSetCurrentTime(const TimeSpec *_pcrtts){
	Thread::specific(currentTimeSpecificPosition(), const_cast<TimeSpec *>(_pcrtts));
}

Object::Object(IndexT _fullid):
	fullid(_fullid), smask(0),
	thrid(0){
}

Object::~Object(){
}

//--------------------------------------------------------------
/*static*/ Object& Object::specific(){
	return *reinterpret_cast<Object*>(Thread::specific(specificPosition()));
}
//--------------------------------------------------------------
void Object::associateToCurrentThread(){
	Thread::specific(specificPosition(), this);
}

bool Object::notify(DynamicPointer<Message> &_rmsgptr){
	return false;//by default do not raise the object
}


int Object::execute(ulong _evs, TimeSpec &_rtout){
	return BAD;
}
//---------------------------------------------------------------------
//----	Signal	----
//---------------------------------------------------------------------
Message::Message(){
	objectCheck<Message>(true, __FUNCTION__);
	vdbgx(Debug::frame, "memadd "<<(void*)this);
}
Message::~Message(){
	objectCheck<Message>(false, __FUNCTION__);
	vdbgx(Debug::frame, "memsub "<<(void*)this);
}

void Message::ipcReceive(
	ipc::MessageUid&
){
}
uint32 Message::ipcPrepare(){
	return 0;//do nothing - no wait for response
}
void Message::ipcComplete(int _err){
	wdbgx(Debug::frame,"");
}
int Message::execute(
	DynamicPointer<Message> &_rthis_ptr,
	uint32 _evs,
	MessageSteward &,
	const MessageUidT &,
	TimeSpec &_rts
){
	wdbgx(Debug::frame, "Unhandled message");
	return BAD;
}

int Message::receiveMessage(
	DynamicPointer<Message> &_rmsgptr,
	const ObjectUidT& _from,
	const ipc::ConnectionUid *_conid
){
	wdbgx(Debug::frame, "Unhandled receiveMessage");
	return BAD;//no need for execution
}

}//namespace frame
}//namespace solid

