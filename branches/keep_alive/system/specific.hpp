/* Declarations file specific.hpp
	
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

#ifndef SPECIFIC_HPP
#define SPECIFIC_HPP

#include <cstddef>
// #include "common.h"
// #include "debug.h"

#ifndef BUF_CACHE_CAP
#define BUF_CACHE_CAP 11
#endif


template <class T, unsigned V = 32>
struct Cacheable: T{
	Cacheable(){}
	~Cacheable(){}
	static unsigned specificCount(){return V;}
	void specificRelease(){this->clear();}
};

struct SpecificObject{
	static void operator delete (void *_p, std::size_t _sz);
	static void* operator new (std::size_t _sz);
	//placement new
	/*inline static void* operator new (std::size_t _sz, void *_pv){
		return ::operator new (_sz, _pv);
	}*/
	virtual ~SpecificObject(){}
};

//! A cache control plugin for Specific.
/*!
	It will be used by specific to control how many buffers
	of a certain capacity should be kept
*/
struct SpecificCacheControl{
	virtual ~SpecificCacheControl(){}
	//! Must return how many buffers should be kept for a specific capacity
	virtual unsigned stackCapacity(unsigned _bufid)const = 0;
	//! If it returns true specific will delete this object
	virtual bool release() = 0;
};

//! A thread specific container wrapper
/*!
	It can cache:<br>
	- objects given by pointers uncache/cache/tryUncache
	- buffers of size power of 2
*/

class Specific{
	/*
		Why it is not a good idea to save only the sizeof a type.
		Think of a btree object.. lots of buffers loaded, saving only the size of an object
		would not be of too much help.
		Also think of objects with sizes larger then max buf capacity
	*/
	template <typename T>
	static T* object(T *_p = NULL){
		static const unsigned id(stackid(&Specific::cleaner<T>));
		if(_p){
			if(push(_p, id, T::specificCount())) delete _p;
		}else{
			_p = reinterpret_cast<T*>(pop(id));
		}
		return _p;
	}
public:
	typedef void (*FncTp) (void*);
	//! call this method to prepare the current thread for caching
	static void prepareThread(SpecificCacheControl *_pscc = NULL);
	//object caching
	//! Uncache an object
	/*! 
		If no object was previously cached, a new object will be created
		using the default constructor.
	*/
	template <class T>
	inline static T* uncache(){
		T *p = object<T>();
		if(!p) p = new T; 
		return p;
	}
	//! Tries to uncache an object
	/*! 
		If no object was previously cached NULL is returned.
	*/
	template <class T>
	inline static T* tryUncache(){
		T *p = object<T>();
		return p;
	}
	//! Caches an object
	template <class T>
	inline static void cache(T *&_rp){
		if(!_rp) return;
		_rp->specificRelease();
		object<T>((T*)_rp);
		_rp = NULL;
	}
	enum {Count = BUF_CACHE_CAP};
	//8,16,32,64,128,256,512,1024,2048,4096
	//! Returns the maximum capacity of cacheable buffers
	static unsigned maxCapacity(){return 1 << (Count + 2);}
	//! Returns the id asociated to the maximum capacity
	static unsigned maxCapacityToId(){return Count - 1;}
	//! Returns the id associated to a certain capacity
	static unsigned capacityToId(unsigned _sz);
	//! Return the id associated to a certain size
	/*!
		A capacity is a power of 2. While a size can have any value.
		Basicaly it return the id of the first capacity greater then size.
	*/
	static unsigned sizeToId(unsigned _sz);
	//! Returns the capacity associated to an id.
	static unsigned idToCapacity(unsigned _id){return 4 << _id;}
	
	//! pop a buffer given its id
	static char* popBuffer(unsigned _id);
	//! pushes a buffer given its id
	static void pushBuffer(char *&, unsigned _id);
// 	static char* allocateBuffer(unsigned _sz);
// 	static void releaseBuffer(char *&, unsigned);
	
private:
	//! One cannot create a Specific object - use prepareThread instead
	Specific();
	Specific(const Specific&);
	~Specific();
private:
	static unsigned stackid(FncTp _pf);
	template<class T>
	static void cleaner(void *_p){
		delete reinterpret_cast<T*>(_p);
	}
	static int push(void *, unsigned, unsigned);
	static void* pop(unsigned);
};



#endif
