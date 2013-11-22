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

class BaseStore: Dynamic<BaseStore, Object>{
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
private:
	/*virtual*/ int execute(ulong _evs, TimeSpec &_rtout);
};

struct BasePointer{
	const UidT& id()const{
		return uid;
	}
private:
	friend class BaseSharedStore;
	UidT	uid;
};

struct AlivePointer{
};


template <
	class T
>
struct ReadPointer{
	
};

template <
	class T
>
struct WritePointer{
	
};

enum Flags{
	SynchronousTryFlag = 1
};

template <
	class T
>
class Store: public Dynamic<Store, BaseStore>{
public:
	AlivePointer aliveInsert(T &_rt){
		
	}
	ReadPointer<T>  readInsert(T &_rt){
		
	}
	WritePointer<T> writeInsert(T &_rt){
		
	}
	
	//!Return false if the object does not exist
	template <typename F>
	bool alive(F _f, UidT const & _ruid, const size_t _flags = SynchronousTryFlag){
		
	}
	
	//!Return false if the object does not exist
	template <typename F>
	bool read(F _f, UidT const & _ruid, const size_t _flags = 0){
		
	}
	//!Return false if the object does not exist
	template <typename F>
	bool write(F _f, UidT const & _ruid, const size_t _flags = 0){
		
	}
};

}//namespace shared
}//namespace frame
}//namespace solid

#endif
