/* Declarations file mutexpool.hpp
	
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

#ifndef MUTEXPOOLPP_HPP
#define MUTEXPOOLPP_HPP

#include <iostream>
#include "system/mutex.hpp"

template <ushort V>
class FastMutexPool{
        enum {sz = (V > 10)?1023:((V<2)?(2):((1<<V)-1))};
public:
        FastMutexPool(){
                //std::cout<<"sz = "<<sz<<std::endl;
                for(ulong i=0;i<(sz+1);++i) pool[i] = NULL;
        }
        ~FastMutexPool(){
                Mutex::Locker lock(mutex);
                for(ulong i=0;i<(sz+1);++i) delete pool[i];
        }
        Mutex *getp(const void *_ptr){
                ulong pos = ((ulong)_ptr) & sz;
                Mutex *m;
                mutex.lock();
                if((m = pool[pos]) == NULL){
                        m = pool[pos] = new Mutex;
                }
                mutex.unlock();
                return m;
        }
        Mutex &getr(const void *_ptr){
                ulong pos = ((ulong)_ptr) & sz;
                Mutex *m;
                mutex.lock();
                if((m = pool[pos]) == NULL){
                        m = pool[pos] = new Mutex;
                }
                mutex.unlock();
                return *m;
        }
private:
        Mutex mutex;
        Mutex *pool[sz+1];
};
/*
class MutexPool{
public:
	MutexPool(unsigned _objpermutbts = 6, unsigned _mutrowsbts = 8, unsigned _mutcolsbts = 8);
	~MutexPool();
	Mutex& mutex(unsigned i);
	Mutex& safeMutex(unsigned i);
	Mutex& operator[](unsigned i);
	int isRangeEnd(unsigned i);
private:
	Mutex& doGetMutex(unsigned i);
	unsigned getMutexRow(unsigned i);
private:
	const unsigned		objpermutbts;//objects per mutex mask
	const unsigned		objpermutmsk;//objects per mutex count
	const unsigned 		mutrowsbts;//mutex rows bits
	const unsigned		mutrowsmsk;//mutex rows mask
	const unsigned		mutrowscnt;//mutex rows count
	const unsigned 		mutcolsmsk;//mutex columns mask
	const unsigned 		mutcolscnt;//mutex columns count
	Mutex				**mutmat;
};

inline Mutex& MutexPool::mutex(unsigned i){
	return doGetMutex(i >> objpermutbts);
}
inline Mutex& MutexPool::doGetMutex(unsigned i){
	return mutmat[(i >> mutrowsbts) & mutrowsmsk][i & mutcolsmsk];
}
inline unsigned MutexPool::getMutexRow(unsigned i){
	return ((i >> objpermutbts) >> mutrowsbts) & mutrowsmsk;
}
inline Mutex& MutexPool::operator[](unsigned i){
	return doGetMutex(i);
}
inline int MutexPool::isRangeEnd(unsigned i){
	return !(i & objpermutmsk);
}
*/
#endif

