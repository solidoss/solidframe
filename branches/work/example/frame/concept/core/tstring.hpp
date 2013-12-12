// tstring.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef CONCEPT_CORE_TSTRING_HPP
#define CONCEPT_CORE_TSTRING_HPP

#include <string>
#include "system/common.hpp"

#define APPSTR(str) str,sizeof(str)-1
namespace solid{
typedef std::string String;

void append(String &_str, solid::ulong _v);
}

#endif
