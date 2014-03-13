// frame/sharedstore.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#ifndef SOLID_FRAME_SHARED_STORE_HPP
#define SOLID_FRAME_SHARED_STORE_HPP

#include "frame/common.hpp"
#include "frame/object.hpp"
#include "utility/dynamictype.hpp"
#include "system/error.hpp"
#include "system/mutex.hpp"

namespace solid{
namespace frame{
namespace shared{

class StoreBase: public Dynamic<StoreBase, Object>{
protected:
	enum Kind{
		UniqueE,
		SharedE
	};
	struct WaitNode{
		WaitNode *pnext;
	};
	struct Stub{
		size_t		alivecnt;
		size_t		usecnt;
		uint8		state;
		WaitNode	*pfirst;
		WaitNode	*plast;
	};
	Mutex &mutex(){
		static Mutex m;
		return m;//TODO!!
	}
	Mutex &mutex(const size_t _idx){
		static Mutex m;
		return m;//TODO!!
	}
	void release(const UidT &_ruid);
	
	size_t doAllocateIndex(){
		return 0;
	}
private:
	/*virtual*/ int execute(ulong _evs, TimeSpec &_rtout);
};

struct PointerBase{
	const UidT& id()const{
		return uid;
	}
protected:
	PointerBase(StoreBase *_psb = NULL):psb(_psb){}
private:
	friend class StoreBase;
	UidT		uid;
	StoreBase	*psb;
};

template <
	class T
>
struct Pointer: PointerBase{
	Pointer(StoreBase *_psb = NULL):PointerBase(_psb), pt(NULL){}
	bool empty()const{
		return pt == NULL;
	}
	T* release(){
		T *p = pt;
		pt = NULL;
		return p;
	}
private:
	T	*pt;
};


enum Flags{
	SynchronousTryFlag = 1
};

/*
 * NOTE: 
 * F signature should be _f(ContextT &)
 */

template <
	class T,
	class Ctl
>
class Store: public Dynamic<Store<T, Ctl>, StoreBase>{
public:
	typedef Pointer<T>	PointerT;
	typedef Ctl			ControllerT;
	
	Store(){}
	
	PointerT	insertAlive(T &_rt){
		
	}
	
	PointerT	insertShared(T &_rt){
	
	}
	
	PointerT	insertUnique(T &_rt){
		
	}
	
	PointerT	insertAlive(){
		
	}
	
	PointerT	insertShared(){
		
	}
	
	PointerT	insertUnique(){
		
	}
	
	//Try get an alive pointer for an intem
	PointerT	alive(UidT const & _ruid, ERROR_NS::error_code &_rerr){
		
	}
	
	//Try get an unique pointer for an item
	PointerT	unique(UidT const & _ruid, ERROR_NS::error_code &_rerr){
		
	}
	
	//Try get a shared pointer for an item
	PointerT	shared(UidT const & _ruid, ERROR_NS::error_code &_rerr){
		
	}
	
	//! Return true if the _f was called within the current thread
	template <typename F>
	bool requestReinit(F &_f, size_t _flags = 0){
		PointerT				uniptr(this);
		size_t					idx = -1;
		ERROR_NS::error_code	err;
		{
			Locker<Mutex>	lock(this->mutex());
			
			if(!_f.prepareIndex(controller(), idx, _flags, err)){
				//an index was not found - need to allocate one
				idx = this->doAllocateIndex();
			}
			if(!err){
				Locker<Mutex>	lock(this->mutex(idx));
				uniptr = doTryGetUnique(idx);
				if(uniptr.empty()){
					doPushWait(_f, StoreBase::UniqueE);
				}else if(!_f.preparePointer(controller(), uniptr, _flags, err)){
					cassert(uniptr.empty());
					doPushWait(_f, StoreBase::UniqueE);
				}
			}
		}
		if(!uniptr.empty() || err){
			_f(controller(), uniptr, err);
			return true;
		}
		return false;
	}

	//! Return true if the _f was called within the current thread
	template <typename F>
	bool requestShared(F _f, UidT const & _ruid, const size_t _flags = 0){
		return false;
	}
	//! Return true if the _f was called within the current thread
	template <typename F>
	bool requestUnique(F _f, UidT const & _ruid, const size_t _flags = 0){
		return false;
	}
	//! Return true if the _f was called within the current thread
	//_f will be called uniquely when object's alive count is zero
	template <typename F>
	bool requestReinit(F _f, UidT const & _ruid, const size_t _flags = 0){
		return false;
	}
	
	ControllerT	&controller(){
		return *static_cast<ControllerT*>(this);
	}
private:
	PointerT doTryGetUnique(const size_t _idx){
		return PointerT();
	}
	template <typename F>
	void doPushWait(F &_f, const StoreBase::Kind _k){
		
	}
};

}//namespace shared
}//namespace frame
}//namespace solid

#endif
