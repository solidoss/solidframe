// betarespnses.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef BETARESPONSES_HPP
#define BETARESPONSES_HPP

#include "system/common.hpp"
#include <string>

using solid::int32;
using solid::uint32;
using solid::uint64;

namespace concept{
namespace beta{
namespace response{
	
struct Basic{
	enum {Identifier = 1};
	int32	error;
	
	Basic():error(0){}
	template <class S>
	S& operator&(S &_s){
		return _s.push(error, "error");
	}
};

struct Test{
	enum {Identifier = 1000};
	int32			count;
	std::string		token;
	
	Test():count(0){}
	
	template <class S>
	S& operator&(S &_s){
		return _s.template pushReinit<Test, 0>(this, 0).push(count, "error");
	}
	
	template <class S, uint32 I>
	int serializationReinit(S &_rs, const uint64 &_rv){
		_rs.pop();
		if(count > 0){
			_rs.push(token, "token");
		}
		return solid::CONTINUE;
	}
};

}//namespace response
}//namespace beta
}//namespace concept

#endif
