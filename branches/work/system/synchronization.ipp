// system/synchronization.ipp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifdef NINLINES
#define inline
#include "system/synchronization.hpp"
#endif

inline Semaphore::Semaphore(int _cnt){
	sem_init(&sem,0,_cnt);
}
inline Semaphore::~Semaphore(){
	sem_destroy(&sem);
}
inline void Semaphore::wait(){
	sem_wait(&sem);
}
inline Semaphore::operator int () {	
	int v;
	sem_getvalue(&sem,&v);
	return v;
}
inline Semaphore &Semaphore::operator++(){
	sem_post(&sem);
	return *this;
}
inline int Semaphore::tryWait(){
	return sem_trywait(&sem);
}

#ifdef NINLINES
#undef inline
#endif

