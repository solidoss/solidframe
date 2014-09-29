// system/specific.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_SPECIFIC_HPP
#define SYSTEM_SPECIFIC_HPP

#include <cstddef>
#include <new>

#include "system/common.hpp"

// #ifndef BUF_CACHE_CAP
// #define BUF_CACHE_CAP 11
// #endif


namespace solid{

//! A base class for thread specific objects
/*!
	A thread specific object is allocated and destroyed by one and the same thread.
	It will try to reuse a buffer from thread's cache, and on delete (which must be
	called on the same thread as new), it will recache the data
*/
struct SpecificObject{
	static void operator delete (void *_p, std::size_t _sz);
	static void* operator new (std::size_t _sz);
	//placement new
	/*inline static void* operator new (std::size_t _sz, void *_pv){
		return ::operator new (_sz, _pv);
	}*/
	//virtual ~SpecificObject(){}
};

struct ObjectCacheConfiguration{
	typedef void (*CleanupFncT) (void*);
	
	ObjectCacheConfiguration(
		size_t _cp = 0, CleanupFncT _pfnc = NULL
	):capacity(_cp), pcleanfnc(_pfnc){
		
	}
	
	size_t		capacity;
	CleanupFncT	pcleanfnc;
};

//! A thread specific container wrapper
/*!
	It can cache:<br>
	- objects given by pointers uncache/cache/tryUncache
	- buffers of size power of 2
*/
class Specific{
public:
	
	//! call this method to prepare the current thread for caching
	static void prepareThread(
		ObjectCacheConfiguration const &_robjcachecfg
	);
	
	static Specific& the();

	
	template <class T>
	
	
	template <class T>
	inline T* pop(){
		T *p = object<T>();
		return p;
	}
	
	template <class T>
	inline void push(T *_p){
		if(_p){
			object<T>(_p);
		}
	}
	template <class T>
	void configure(ObjectCacheConfiguration const &_robjcachecfg){
		object<T>(NULL, &_robjcachecfg);
	}
	
	void *allocate(size_t _cp);
	
	void free(void *_p, size_t _cp);
	
private:
	typedef ObjectCacheConfiguration::CleanupFncT CleanupFncT;
	
	static size_t stackid(CleanupFncT _pf);
	
	template<class T>
	static void cleaner(void *_p){
		delete reinterpret_cast<T*>(_p);
	}
	
	void* doPopObject(const size_t _id);
	bool  doPushObject(const size_t _id, void *_pv);
	void doConfigure(ObjectCacheConfiguration const &_rcfg);
	
	/*
		Why it is not a good idea to save only the sizeof a type.
		Think of a btree object.. lots of buffers loaded, saving only the size of an object
		would not be of too much help.
	*/
#ifdef HAS_SAFE_STATIC
	template <typename T>
	static size_t objectTypeId(){
		static const size_t id = stackid(&Specific::template cleaner<T>);
		return id;
	}
	
	template <typename T>
	T* object(T *_p = NULL, const ObjectCacheConfiguration *_pcfg){
		static const size_t id = stackid(&Specific::template cleaner<T>);
		if(_p){
			doPushObject(id, _p);
		}else if(!_pcfg){
			_p = reinterpret_cast<T*>(doPopObject(id));
			_p = NULL;
		}else{
			doConfigure(*_pcfg);
		}
		
		return _p;
	}
#else
	template <typename T>
	T* object_stub(T *_p = NULL){
		static const size_t id(stackid(&Specific::cleaner<T>));
		if(_p){
			doPushObject(id, _p);
		}else if(!_pcfg){
			_p = reinterpret_cast<T*>(doPopObject(id));
			_p = NULL;
		}else{
			doConfigure(*_pcfg);
		}
		
		return _p;
	}
	template <typename T>
	static void once_cbk(){
			T *p = object_stub();
			objec_stub(p);
	}
	template <typename T>
	static T* object(T *_p = NULL){
		static boost::once_flag	once = BOOST_ONCE_INIT;
		boost::call_once(&once_cbk<T>, once);
		return objec_stub<T>(_p);
	}
#endif
private:
	//! One cannot create a Specific object - use prepareThread instead
	Specific();
	Specific(const Specific&);
	~Specific();
	Specific& operator=(const Specific&);
	
	struct Data;
	Data	&d;
};

}//namespace solid

#endif
