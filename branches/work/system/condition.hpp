// system/condition.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_CONDITION_HPP
#define SYSTEM_CONDITION_HPP

#include "system/common.hpp"
#include "system/cassert.hpp"

#if 	defined(USTLMUTEX)
#include "system/condition_stl.hpp"
#elif	defined(UBOOSTMUTEX)
#include "system/condition_boost.hpp"
#else

#include <pthread.h>

namespace solid{

struct TimeSpec;
struct Mutex;
template <class M>
struct Locker;
//! A simple posix condition wrapper
class Condition{
public:
	Condition();
	~Condition();
	//! Try to wake one waiting thread
	int signal();
	//! Try to wake all waiting threads
	int broadcast();
	//! Wait for a signal
	int wait(Locker<Mutex> &_lock);
	//! Wait for a signal a certain amount of time
	int wait(Locker<Mutex> &_lock, const TimeSpec &_ts);
private:
	pthread_cond_t cond;
};

}//namespace solid

#ifndef NINLINES
#include "system/mutex.hpp"
namespace solid{
#include "system/condition.ipp"
}//namespace solid
#endif


#endif

#endif
