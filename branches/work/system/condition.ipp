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
	cverify(!pthread_cond_init(&cond, NULL));
}
inline Condition::~Condition(){
	cverify(!pthread_cond_destroy(&cond));
}

inline void Condition::signal(){
	cverify(!pthread_cond_signal(&cond));
}
inline void Condition::broadcast(){
	cverify(!pthread_cond_broadcast(&cond));
}
inline void Condition::wait(Locker<Mutex> &_lock){
	cverify(!pthread_cond_wait(&cond, &_lock.m.mut));
}
#ifdef NINLINES
#undef inline
#endif
