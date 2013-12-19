// system/error.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_SYSTEM_ERROR_HPP
#define SOLID_SYSTEM_ERROR_HPP

#include "config.h"

#ifdef HAS_CPP11
#define ERROR_NS std
#else
#define ERROR_NS boost::system
#endif

#ifdef HAS_CPP11
#include <system_error>
#else
#include "boost/system/error_code.hpp"
#endif

namespace solid{

ERROR_NS::error_code last_system_error();
ERROR_NS::error_code last_error();
void last_error(int _err);

enum Errors{
	ERROR_NOERROR = 0,
	ERROR_NOT_IMPLEMENTED,
	ERROR_SYSTEM,
};

}//namespace solid

#endif
