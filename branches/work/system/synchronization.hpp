// system/synchronization.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_SYNCHRO_HPP
#define SYSTEM_SYNCHRO_HPP

#include "system/cassert.hpp"
#ifndef ON_WINDOWS
#include <semaphore.h>


namespace solid{

struct TimeSpec;

class Semaphore{
public:
	Semaphore(int _cnt = 0);
	~Semaphore();
	void wait();
	operator int ();
	Semaphore & operator++();
	int tryWait();
private:
	sem_t sem;
};
#ifndef NINLINES
#include "system/synchronization.ipp"
#endif

}//namespace solid
#endif

#endif


