// betasignals.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef BETASIGNALS_HPP
#define BETASIGNALS_HPP

#include <string>
#include <sstream>

#include "frame/message.hpp"

namespace concept{
namespace beta{

struct SignalWaiter{
	virtual void signal() = 0;
	
	std::ostringstream	oss;
};

struct LoginMessage: solid::Dynamic<LoginMessage, solid::frame::Message>{
	LoginMessage(SignalWaiter &_rsw):rsw(_rsw){}
	size_t release();
	SignalWaiter	&rsw;
	std::string		user;
	std::string		pass;
};

struct CancelMessage: solid::Dynamic<CancelMessage, solid::frame::Message>{
	CancelMessage(SignalWaiter &_rsw):rsw(_rsw){}
	size_t release();
	SignalWaiter	&rsw;
	solid::uint32	tag;
};

}//namespace beta
}//namespace concept


#endif
