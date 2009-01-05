/* Declarations file common.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TESTCOMMON_HPP
#define TESTCOMMON_HPP

#include "foundation/core/common.hpp"

#include <string.h>

typedef foundation::RequestUidTp	RequestUidTp;
typedef foundation::FileUidTp		FileUidTp;
typedef foundation::ObjectUidTp	ObjectUidTp;
typedef foundation::CommandUidTp	CommandUidTp;

struct StrLess{
	bool operator()(const char* const &_str1, const char* const &_str2)const{
		return strcasecmp(_str1,_str2) < 0;
	}
};


#endif
