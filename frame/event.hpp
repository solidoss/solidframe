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

#include <ostream>
#include "frame/common.hpp"
#include "utility/dynamictype.hpp"
#include "utility/dynamicpointer.hpp"

namespace solid{
namespace frame{

typedef DynamicPointer<>				MessagePointerT;

struct Event;

class EventCategoryBase: public Dynamic<EventCategoryBase>{
private:
	friend struct Event;
	virtual void print(std::ostream &_ros, Event const &_re)const;
	
protected:
	Event doCreate(size_t _id)const;
	const size_t eventId(Event const&_re)const;
	EventCategoryBase(){}
	bool check(Event const&_re)const;
private:
	EventCategoryBase(EventCategoryBase const&);
	EventCategoryBase& operator=(EventCategoryBase const&);
};


struct EventCategory: public Dynamic<EventCategory, EventCategoryBase>{
	enum EventId{
		KillE,
		StartE,
		StopE,
		RaiseE,
		MessageE,
		TimerE,
		InvalidE
	};
	
	static EventId id(Event const&_re);
	
	static bool isKill(Event const&_re);
	static bool isStart(Event const&_re);
	static bool isStop(Event const&_re);
	static bool isRaise(Event const&_re);
	static bool isMessage(Event const&_re);
	static bool isTimer(Event const&_re);
	
	
	static Event create(EventId _evid);
	
	static Event createStart();
	static Event createKill();
	static Event createStop();
	static Event createRaise();
	static Event createMessage();
	static Event createTimer();
	
	static EventCategory const& the();

private:
	/*virtual*/ void print(std::ostream &_ros, Event const &_re)const;
};


struct Event{
	uint64				value;
	MessagePointerT		msgptr;
	
	EventCategoryBase const& category()const{
		return *pcategory;
	}
	
	Event& assign(uint64 const& _rv, MessagePointerT const &_rmsgptr = MessagePointerT()){
		value = _rv;
		msgptr = _rmsgptr;
		return *this;
	}
	
	Event(
		Event const &_re
	): value(_re.value), msgptr(_re.msgptr), pcategory(_re.pcategory), id(_re.id){}
	
	Event(
		Event &&_re
	): value(_re.value), msgptr(std::move(_re.msgptr)), pcategory(_re.pcategory), id(_re.id){
		_re.clear();
	}
	
	Event():value(0), pcategory(&EventCategory::the()), id(static_cast<size_t>(EventCategory::InvalidE)){
		
	}
	
	Event &operator=(Event const &_re){
		pcategory = _re.pcategory;
		id = _re.id;
		value = _re.value;
		msgptr = _re.msgptr;
		return *this;
	}
	
	Event &operator=(Event &&_re){
		pcategory = _re.pcategory;
		id = _re.id;
		value = _re.value;
		msgptr = std::move(_re.msgptr);
		_re.clear();
		return *this;
	}
	
	void clear(){
		value = 0;
		id = -1;
		msgptr.clear();
		pcategory = &EventCategory::the();
	}
	
	std::ostream & print(std::ostream &_ros)const{
		pcategory->print(_ros, *this);
		return _ros;
	}
	
private:
	friend EventCategoryBase;
	Event(
		EventCategoryBase const& _rcategory,
		const size_t _id
	):value(0), pcategory(&_rcategory), id(_id){}
	
private:
	const EventCategoryBase	*pcategory;
	
	size_t					id;
	
	
};

std::ostream& operator<<(std::ostream &_ros, Event const&_re);

inline Event EventCategoryBase::doCreate(size_t _id)const{
	return Event(*this, _id);
}
inline const size_t EventCategoryBase::eventId(Event const&_re)const{
	return _re.id;
}

inline bool EventCategoryBase::check(Event const&_re)const{
	return (this == _re.pcategory);
}

/*static*/ inline EventCategory::EventId EventCategory::id(Event const&_re){
	if(the().check(_re)){
		return static_cast<EventId>(the().eventId(_re));
	}else{
		return InvalidE;
	}
}

/*static*/ inline Event EventCategory::create(EventId _evid){
	return the().doCreate(static_cast<size_t>(_evid));
}

/*static*/ inline bool EventCategory::isKill(Event const&_re){
	return id(_re) == KillE;
}
/*static*/ inline bool EventCategory::isStart(Event const&_re){
	return id(_re) == StartE;
}
/*static*/ inline bool EventCategory::isStop(Event const&_re){
	return id(_re) == StopE;
}
/*static*/ inline bool EventCategory::isRaise(Event const&_re){
	return id(_re) == RaiseE;
}
/*static*/ inline bool EventCategory::isMessage(Event const&_re){
	return id(_re) == MessageE;
}
/*static*/ inline bool EventCategory::isTimer(Event const&_re){
	return id(_re) == TimerE;
}

/*static*/ inline Event EventCategory::createStart(){
	return create(StartE);
}
/*static*/ inline Event EventCategory::createKill(){
	return create(KillE);
}
/*static*/ inline Event EventCategory::createStop(){
	return create(StopE);
}
/*static*/ inline Event EventCategory::createRaise(){
	return create(RaiseE);
}
/*static*/ inline Event EventCategory::createMessage(){
	return create(MessageE);
}
/*static*/ inline Event EventCategory::createTimer(){
	return create(TimerE);
}

}//namespace frame
}//namespace solid

#endif
