// system/mutex.ipp
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
#include "system/cassert.hpp"
#endif

inline Mutex::Mutex(){
	pthread_mutexattr_t att;
	pthread_mutexattr_init(&att);
#ifdef UDEBUG
	pthread_mutexattr_settype(&att, (int)ERRORCHECK);
#else
	pthread_mutexattr_settype(&att, (int)FAST);
#endif
	pthread_mutex_init(&mut,&att);
	pthread_mutexattr_destroy(&att);
}

inline Mutex::Mutex(Type _type){
	pthread_mutexattr_t att;
	pthread_mutexattr_init(&att);
	pthread_mutexattr_settype(&att, (int)_type);
	pthread_mutex_init(&mut,&att);
	pthread_mutexattr_destroy(&att);
}
inline Mutex::~Mutex(){
	pthread_mutex_destroy(&mut);
}

inline void Mutex::lock(){
#ifdef UDEBUG
#ifdef NINLINES
	int rv = pthread_mutex_lock(&mut);
	if(rv){
		edbgx(Debug::system, "pthread_mutex_lock: "<<strerror(errno));
		cassert(!rv);
	}
#else
	pthread_mutex_lock(&mut);
#endif
#else
	pthread_mutex_lock(&mut);
#endif
}
inline void Mutex::unlock(){
#ifdef UDEBUG
#ifdef NINLINES
	int rv = pthread_mutex_unlock(&mut);
	if(rv){
		edbgx(Debug::system, "pthread_mutex_unlock: "<<strerror(errno));
	}
	cassert(!rv);
#else
	pthread_mutex_unlock(&mut);
#endif
#else
	pthread_mutex_unlock(&mut);
#endif
}

#ifdef _WIN32
inline bool Mutex::tryLock(){
}
#else
inline bool Mutex::tryLock(){
	return pthread_mutex_trylock(&mut)==0;
}
#endif

#ifdef NINLINES
#undef inline
#endif


