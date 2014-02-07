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

namespace solid{
namespace frame{
namespace shared{

class StoreBase: Dynamic<StoreBase, Object>{
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
	Mutex &mutex();
private:
	/*virtual*/ int execute(ulong _evs, TimeSpec &_rtout);
};

struct PointerBase{
	const UidT& id()const{
		return uid;
	}
private:
	friend class StoreBase;
	UidT		uid;
	StoreBase	&rsb;
};

template <
	class T
>
struct Pointer: PointerBase{

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
		
	}
	bool isSynchronous()const;
	const error_code & error()const;
	void error(const error_code &_rerr);
private:
	T			&rt;
	Stub		&rs;
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
class Store: public Dynamic<Store, StoreBase>{
public:
	typedef Pointer<T>	PointerT;
	typedef Context<T>	ContextT;
	typedef Ctl			ControllerT;
	
	Store(const ControllerT &_rctl):ctl(_rctl){}
	
	template <typename T>
	Store(T _t):ctl(_t){}
	
	Store(){}
	
	//returned PointerT::get() == NULL
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
	//TODO: move the final version to shared::Store
	template <typename F>
	bool requestReinit(F &_f, const size_t _flags = 0){
		PointerT		uniptr;
		size_t			idx = -1;
		{
			Locker<Mutex>	lock(mutex());
			
			uniptr = doInsertUnique();
			_f.prepare(uniptr, idx);
			if(uniptr.empty()){
				//the pointer was stored for later use - doRequestReinit will only schedule f until
				//uniptr is cleared
			}else if(idx != static_cast<size_t>(-1) && uniptr.id().first != idx){
				//_f requires another object so we erase the currently allocated one
				doErase(uniptr);
			}else{
				//doRequestReinit will clear uniptr and run _f
			}
		}
		return doRequestReinit(_f, idx, uniptr, _flags);//will use object's mutex
	}

	//!Return false if the object does not exist
	template <typename F>
	bool requestShared(F _f, UidT const & _ruid, const size_t _flags = 0){
		
	}
	//!Return false if the object does not exist
	template <typename F>
	bool requestUnique(F _f, UidT const & _ruid, const size_t _flags = 0){
		
	}
	//!Return false if the object does not exist
	//_f will be called uniquely when object's alive count is zero
	template <typename F>
	bool requestReinit(F _f, UidT const & _ruid, const size_t _flags = 0){
		
	}
protected:
	ControllerT	&controller(){
		return ctl;
	}
private:
	ControllerT		ctl;
};

}//namespace shared
}//namespace frame
}//namespace solid

#endif
