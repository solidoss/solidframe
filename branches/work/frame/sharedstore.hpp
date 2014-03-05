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

template <class T>
struct Context{
	T& operator*(){
		return rt;
	}
	
	Pointer<T> alive(){
	}
	//Will return a pointer based on the current request type.
	//All request types can get alive pointers
	//For unique request one can get either shared or unique pointers.
	//For shared request only shared pointers are allowed.
	//In case of error an invalid pointer is returned.
	Pointer<T> shared(){
	}
	
	Pointer<T> unique(){
	}
	
	Mutex& mutex(){
		static Mutex m;
		return m;//TODO:
	}
	bool isSynchronous()const;
	const ERROR_NS::error_code & error()const;
	void error(const ERROR_NS::error_code &_rerr);
private:
	T			&rt;
	//Stub		&rs;
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
	typedef Context<T>	ContextT;
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
	
	//Get an alive pointer from an existing pointer
	PointerT	alive(PointerT &_rp){
		
	}
	//! Return true if the _f was called within the current thread
	template <typename F>
	bool requestReinit(F &_f, size_t _flags = 0){
		PointerT		uniptr(this);
		size_t			idx = -1;
		{
			Locker<Mutex>	lock(this->mutex());
			
			uniptr = this->doInsertUnique();
			_f.prepare(uniptr, idx, _flags);
			if(uniptr.empty()){
				//the pointer was stored for later use - doRequestReinit will only schedule f until
				//uniptr is cleared
			}else if(uniptr.id().first != idx){
				//_f requires another object so we erase the currently allocated one
				//for idx == -1, we still erase the uniptr and doRequestReinit will call _f with error
				doErase(uniptr);
			}else{
				//doRequestReinit will clear uniptr and run _f
			}
		}
		return doRequestReinit(_f, idx, uniptr, _flags);//will use object's mutex
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
	PointerT doInsertUnique(){
		return PointerT();
	}
	void doErase(PointerT){
		
	}
	template <class F>
	bool doRequestReinit(F &_rf, const size_t _idx, PointerT, size_t _flags){
		return false;
	}
};

}//namespace shared
}//namespace frame
}//namespace solid

#endif
