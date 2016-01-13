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

#ifdef SOLID_USE_CPP11
#define FUNCTION_NS std
#else
#define FUNCTION_NS boost
#endif

#ifdef SOLID_USE_CPP11
#include <functional>
#else
#include "boost/function.hpp"
#endif


#define USE_BOOST_FUNCTION

#ifdef USE_BOOST_FUNCTION
#include "boost/function.hpp"
#define FUNCTION boost::function
#define FUNCTION_EMPTY(f) (f.empty())
#define FUNCTION_CLEAR(f) (f.clear())
#else
#define FUNCTION std::function
#define FUNCTION_EMPTY(f) (f == nullptr)
#define FUNCTION_CLEAR(f) (f = nullptr)
#endif


#endif
