// betarequests.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef BETAREQUESTS_HPP
#define BETAREQUESTS_HPP

#include "system/common.hpp"
#include <string>

using solid::uint32;

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
