/* Declarations file common.hpp
	
	Copyright 2007, 2008, 2013 Valentin Palade 
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

#ifndef CONCEPT_CORE_COMMON_HPP
#define CONCEPT_CORE_COMMON_HPP

#include "frame/common.hpp"

#include <string.h>

typedef solid::frame::RequestUidT		RequestUidT;
typedef solid::frame::FileUidT			FileUidT;
typedef solid::frame::ObjectUidT		ObjectUidT;
typedef solid::frame::MessageUidT		MessageUidT;
typedef solid::frame::IndexT			IndexT;

struct StrLess{
	bool operator()(const char* const &_str1, const char* const &_str2)const{
		return strcasecmp(_str1,_str2) < 0;
	}
};


#endif
