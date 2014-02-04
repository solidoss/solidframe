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
	//Will return a pointer based on the request type
	//One will be able to request multiple pointers from a unique request
	//The status of the object will be "unique" untill all the pointers will
	//be freed
	Pointer<T> pointer(){
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
	class T
>
class Store: public Dynamic<Store, StoreBase>{
public:
	typedef Pointer<T>	PointerT;
	typedef Context<T>	ContextT;
	
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
	
};

}//namespace shared
}//namespace frame
}//namespace solid

#endif
