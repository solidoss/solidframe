// system/atomic.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_SYSTEM_ATOMIC_HPP
#define SOLID_SYSTEM_ATOMIC_HPP

#include "config.h"

#ifdef HAS_CPP11
#define ATOMIC_NS std
#else
#define ATOMIC_NS boost
#endif

#ifdef HAS_CPP11
#include <atomic>
#else
#include "boost/atomic.hpp"
#define ATOMIC_VAR_INIT(a) (a)
#endif

#endif
