/* Declarations file betarequests.hpp
	
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


#ifndef BETAREQUESTS_HPP
#define BETAREQUESTS_HPP

#include "system/common.hpp"
#include <string>

namespace concept{
namespace beta{
namespace request{

struct Noop{
	enum {Identifier = 1};
	template <class S>
	S& operator&(S &_s){
		return _s;
	}
};

struct Login{
	enum {Identifier = 2};
	std::string		user;
	std::string		password;
	
	template <class S>
	S& operator&(S &_s){
		return _s.pushUtf8(password, "password").pushUtf8(user, "user");
	}
};

struct Cancel{
	enum {Identifier = 3};
	uint32	tag;
	
	Cancel():tag(0){}
	template <class S>
	S& operator&(S &_s){
		return _s.push(tag, "tag");
	}
};

struct Test{
	enum {Identifier = 1000};
	uint32		count;
	uint32		timeout;
	std::string	token;
	
	Test():count(0), timeout(0){}
	template <class S>
	S& operator&(S &_s){
		return _s.push(token, "token").push(timeout, "timeout").push(count, "count");
	}
};

}//namespace request
}//namespace beta
}//namespace concept

#endif
