// system/src/mutexpool.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef MUTEXPOOLPP_HPP
#define MUTEXPOOLPP_HPP

#include <iostream>
#include "system/mutex.hpp"

namespace solid{

template <ushort V>
class FastMutexPool{
        enum {sz = (V > 10)?1023:((V<2)?(2):((1<<V)-1))};
public:
        FastMutexPool(){
                //std::cout<<"sz = "<<sz<<std::endl;
                for(ulong i=0;i<(sz+1);++i) pool[i] = NULL;
        }
        ~FastMutexPool(){
                Locker<Mutex> lock(mutex);
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

}//namespace solid

#endif

