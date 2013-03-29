/* Declarations file betarespnses.hpp
	
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


#ifndef BETARESPONSES_HPP
#define BETARESPONSES_HPP

#include "system/common.hpp"
#include <string>

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
		return CONTINUE;
	}
};

}//namespace response
}//namespace beta
}//namespace concept

#endif
