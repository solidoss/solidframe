// common.hpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef CONCEPT_CORE_COMMON_HPP
#define CONCEPT_CORE_COMMON_HPP

#include "frame/common.hpp"

#include <string.h>

//typedef solid::frame::RequestUidT		RequestUidT;
//typedef solid::frame::FileUidT			FileUidT;
typedef solid::frame::ObjectUidT		ObjectUidT;
//typedef solid::frame::MessageUidT		MessageUidT;
typedef solid::frame::IndexT			IndexT;

struct StrLess{
	bool operator()(const char* const &_str1, const char* const &_str2)const{
		return strcasecmp(_str1,_str2) < 0;
	}
};


#endif
