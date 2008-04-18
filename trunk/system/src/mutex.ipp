/* Inline implementation file mutex.ipp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UINLINES
#define inline
#endif

inline Mutex::Locker::Locker(Mutex &_m):m(_m){
	m.lock();
}
inline Mutex::Locker::~Locker(){
	m.unlock();
}
inline Mutex::Mutex(Type _type){
	pthread_mutexattr_t att;
	
	pthread_mutexattr_init(&att);
	pthread_mutexattr_settype(&att, (int)_type);
	pthread_mutex_init(&mut,&att);
}
inline Mutex::~Mutex(){
	if(locked()){
		pthread_mutex_unlock(&mut);
	}
	pthread_mutex_destroy(&mut);
}

inline void Mutex::lock(){
#ifdef UDEBUG
#ifndef UINLINES
	int rv = pthread_mutex_lock(&mut);
	if(rv){
		idbg("lock error "<<strerror(errno));
	}
	cassert(!rv);
#else
	pthread_mutex_lock(&mut);
#endif
#else
	pthread_mutex_lock(&mut);
#endif
}
inline void Mutex::unlock(){
#ifdef UDEBUG
#ifndef UINLINES
	int rv = pthread_mutex_unlock(&mut);
	if(rv){
		idbg("lock error "<<strerror(errno));
	}
	cassert(!rv);
#else
	pthread_mutex_unlock(&mut);
#endif
#else
	pthread_mutex_unlock(&mut);
#endif
}
inline bool Mutex::locked(){
	return pthread_mutex_trylock(&mut) != 0;
}

#ifndef UINLINES
#undef inline
#endif


