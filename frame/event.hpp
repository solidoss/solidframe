// frame/event.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_EVENT_HPP
#define SOLID_FRAME_EVENT_HPP

#include "frame/common.hpp"
#include "frame/message.hpp"

namespace solid{
namespace frame{

typedef DynamicPointer<Message>				MessagePointerT;

//! Some events
enum EventIdE{
	EventUnknown = 0,
	EventInit,
	EventDie,
	EventTimer,
	EventLast = 16
};


struct Event{
	Event(
		const size_t _id = EventUnknown,
		const size_t _idx = 0
	):id(_id), index(_idx){}
	
	Event(
		MessagePointerT const &_rmsgptr,
		const size_t _id = EventUnknown,
		const size_t _idx = 0
	):id(_id), index(_idx), msgptr(_rmsgptr){}
	
	Event(
		Event const &_e
	):id(_e.id), index(_e.index), msgptr(_e.msgptr){}
	
	
	size_t				id;
	size_t				index;
	MessagePointerT		msgptr;
};

}//namespace frame
}//namespace solid

#endif
