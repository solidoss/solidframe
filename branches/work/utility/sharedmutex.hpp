// utility/sharedmutex.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_SHARED_MUTEX_STORE_HPP
#define UTILITY_SHARED_MUTEX_STORE_HPP

namespace solid{

struct Mutex;

Mutex &shared_mutex_safe(const void *_p);
Mutex &shared_mutex(const void *_p);


}//namespace solid

#endif
