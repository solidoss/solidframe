/* Declarations file betasignals.hpp
	
	Copyright 2012 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

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
