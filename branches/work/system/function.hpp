// system/function.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_SYSTEM_FUNCTION_HPP
#define SOLID_SYSTEM_FUNCTION_HPP

#include "config.h"

#ifdef HAS_CPP11
#define FUNCTION_NS std
#else
#define FUNCTION_NS boost
#endif

#ifdef HAS_CPP11
#include <functional>
#else
#include "boost/function.hpp"
#endif

#endif
