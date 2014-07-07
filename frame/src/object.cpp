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

#include "frame/objectbase.hpp"
#include "frame/object.hpp"
#include "frame/message.hpp"
#include "frame/manager.hpp"
#include "frame/service.hpp"
#include "frame/reactor.hpp"
#include "frame/completion.hpp"

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
#else
const uint specificIdStub(){
	static const uint id(Thread::specificId());
	return id;
}

void once_stub(){
	specificIdStub();
}

static const unsigned specificPosition(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_stub, once);
	return specificIdStub();
}
#endif
}

namespace frame{

//---------------------------------------------------------------------
//----	ObjectBase	----
//---------------------------------------------------------------------

ObjectBase::ObjectBase():
	fullid(-1){
}

void ObjectBase::unregister(){
	if(isRegistered()){
		Manager::specific().unregisterObject(*this);
		fullid = -1;
	}
}

ObjectBase::~ObjectBase(){
	unregister();
}

/*virtual*/void ObjectBase::doStop(){
}
void ObjectBase::stop(){
	doStop();
	unregister();
}
//--------------------------------------------------------------
/*static*/ ObjectBase& ObjectBase::specific(){
	return *reinterpret_cast<Object*>(Thread::specific(specificPosition()));
}
//--------------------------------------------------------------
void ObjectBase::prepareSpecific(){
	Thread::specific(specificPosition(), this);
}
//---------------------------------------------------------------------
//----	Object	----
//---------------------------------------------------------------------
static CompletionHandler	dummy_ch;//used for detecting object preparing / running state

Object::Object():pchfirst(&dummy_ch){}

bool Object::isRunning()const{
	return pchfirst != &dummy_ch && (pchfirst == NULL || pchfirst->pprev != &dummy_ch);
}

void Object::enterRunning(){
	cassert(!isRunning());
	if(pchfirst == &dummy_ch){
		pchfirst = NULL;
	}else{
		pchfirst->pprev = NULL;
	}
}

bool Object::registerCompletionHandler(CompletionHandler &_rch){
	const bool rv = isRunning();
	if(rv){
		CompletionHandler *poldfirst = pchfirst;
		pchfirst = &_rch;
		pchfirst->pprev = poldfirst;
		pchfirst->pnext = NULL;
		if(poldfirst){
			poldfirst->pnext = pchfirst;
		}
	}else{
		CompletionHandler *poldfirst = pchfirst;
		pchfirst = &_rch;
		pchfirst->pprev = poldfirst;
		pchfirst->pnext = &dummy_ch;
		if(poldfirst != &dummy_ch){
			poldfirst->pnext = pchfirst;
		}else{
			pchfirst->pprev = NULL;
		}
	}
	return rv;
}

bool Object::unregisterCompletionHandler(CompletionHandler &_rch){
	cassert(_rch.isRegistered());
	const bool rv = isRunning();
	if(rv){
		if(_rch.pprev){
			_rch.pprev->pnext = _rch.pnext;
		}
		if(_rch.pnext){
			_rch.pnext->pprev = _rch.pprev;
		}
		if(&_rch == pchfirst){
			pchfirst = _rch.pprev;
		}
	}else{
		if(_rch.pprev){
			_rch.pprev->pnext = _rch.pnext;
		}
		if(_rch.pnext != &dummy_ch){
			_rch.pnext->pprev = _rch.pprev;
		}
		if(&_rch == pchfirst){
			pchfirst = _rch.pprev;
		}
	}
	_rch.pobj = NULL;
	_rch.pprev = NULL;
	_rch.pnext = NULL;
	return rv;
}
//---------------------------------------------------------------------
//----	Message	----
//---------------------------------------------------------------------
Message::Message(){
	objectCheck<Message>(true, __FUNCTION__);
	vdbgx(Debug::frame, "memadd "<<(void*)this);
}
Message::~Message(){
	objectCheck<Message>(false, __FUNCTION__);
	vdbgx(Debug::frame, "memsub "<<(void*)this);
}
}//namespace frame
}//namespace solid

