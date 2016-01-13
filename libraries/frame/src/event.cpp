// frame/src/event.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "system/cassert.hpp"
#include "frame/event.hpp"



namespace solid{
namespace frame{

/*virtual*/ void EventCategoryBase::print(std::ostream &_ros, Event const &_re)const{
	_ros<<eventId(_re);
}

/*virtual*/ void EventCategory::print(std::ostream &_ros, Event const &_re)const{
	const char *pstr;
	switch(eventId(_re)){
		case KillE:
			pstr = "frame::KillE";break;
		case StartE:
			pstr = "frame::StartE";break;
		case StopE:
			pstr = "frame::StopE";break;
		case RaiseE:
			pstr = "frame::RaiseE";break;
		case MessageE:
			pstr = "frame::MessageE";break;
		case InvalidE:
			pstr = "frame::InvalidE";break;
		default:
			_ros<<"frame::Unkonwn::"<<eventId(_re);
			return;
	}
	_ros<<pstr;
}

std::ostream& operator<<(std::ostream &_ros, Event const&_re){
	return _re.print(_ros);
}

/*static*/ EventCategory const& EventCategory::the(){
	static const EventCategory c;
	return c;
}


}//namespace frame
}//namespace solid

