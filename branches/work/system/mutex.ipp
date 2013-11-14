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
#ifdef UDEBUG
	pthread_mutexattr_t att;
	pthread_mutexattr_init(&att);
	pthread_mutexattr_settype(&att, (int)ERRORCHECK);
	pthread_mutex_init(&mut,&att);
	pthread_mutexattr_destroy(&att);
#else
	//pthread_mutexattr_settype(&att, (int)FAST);
	pthread_mutex_init(&mut, NULL);
#endif
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

inline SharedMutex::SharedMutex(){
	 pthread_rwlock_init(&mut, NULL);
}
inline SharedMutex::~SharedMutex(){
	pthread_rwlock_destroy(&mut);
}
inline void SharedMutex::lock(){
	pthread_rwlock_wrlock(&mut);
}
inline void SharedMutex::unlock(){
	 pthread_rwlock_unlock(&mut);
}
inline bool SharedMutex::tryLock(){
	return pthread_rwlock_trywrlock(&mut) == 0;
}
inline void SharedMutex::sharedLock(){
	pthread_rwlock_rdlock(&mut);
}
inline void SharedMutex::sharedUnlock(){
	 pthread_rwlock_unlock(&mut);
}
inline bool SharedMutex::sharedTryLock(){
	return pthread_rwlock_tryrdlock(&mut) == 0;
}

#ifdef _WIN32
inline bool Mutex::tryLock(){
	return false;
}
#else
inline bool Mutex::tryLock(){
	return pthread_mutex_trylock(&mut)==0;
}
#endif

#ifdef NINLINES
#undef inline
#endif


