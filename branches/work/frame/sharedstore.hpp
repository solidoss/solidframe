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

namespace solid{
namespace frame{


template <
	class T
>
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


template <
	class T
>
class SharedStore{
public:
	AlivePointer<T> insertAlive(T &_rt);
	ReadPointer<T>  insertRead(T &_rt);
	WritePointer<T> insertWrite(T &_rt);
	
	template <typename F>
	void asyncRead(UidT const & _ruid, F _f){
		
	}
	template <typename F>
	void asyncWrite(UidT const & _ruid, F _f){
		
	}
};

}//namespace frame
}//namespace solid

#endif
