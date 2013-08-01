// system/condition.ipp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifdef NINLINES
#define inline
#else
#include "system/mutex.hpp"
#endif


inline Condition::Condition(){
	int rv = pthread_cond_init(&cond,NULL);
	cassert(rv == 0);
}
inline Condition::~Condition(){
	int rv = pthread_cond_destroy(&cond);
	cassert(rv==0);
}
inline int Condition::signal(){
	return pthread_cond_signal(&cond);
}
inline int Condition::broadcast(){
	return pthread_cond_broadcast(&cond);
}
inline int Condition::wait(Locker<Mutex> &_lock){
	return pthread_cond_wait(&cond, &_lock.m.mut);
}

#ifdef NINLINES
#undef inline
#endif
