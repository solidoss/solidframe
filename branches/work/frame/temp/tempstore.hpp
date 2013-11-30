// frame/temp/tempstore.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_TEMP_STORE_HPP
#define SOLID_FRAME_TEMP_STORE_HPP

#include "frame/sharedstore.hpp"

namespace solid{
namespace frame{
namespace temp{

enum{
	MemoryLevelFlag = 1,
	VeryFastLevelFlag = 2,
	FastLevelFlag = 4,
	NormalLevelFlag = 8,
	SlowLevelFlag = 16,
	AllLevelFlags = 1 + 2 + 4 + 8 + 16,
};

struct Configuration{
	struct Storage{
		std::string 	path;
		uint32			level;
		uint64			capacity;
	};
	typedef std::vector<Storage>	StorageVectorT;
	
	StorageVectorT	storagevec;
};

typedef shared::AlivePointer	AlivePointerT;
typedef shared::WritePointer<>	WritePointerT;
typedef shared::ReadPointer<>	ReadPointerT;

/*
 * NOTE: all request methods return true if _f(...) was called synchronously
 * F signature should be _f(PointerT &_ptr, error_code _err, bool _synchronous)
 */
class Store: protected shared::Store<>{
public:
	Store(Configuration const &_rcfg);
	template <typename F>
	bool requestCreateAlive(F _f, uint64 _sz, const size_t _flags = AllLevelFlags){
		
	}
	
	template <typename F>
	bool requestCreateAlive(F _f, uint64 _sz, AlivePointerT &_alvptr, const size_t _flags = AllLevelFlags){
		
	}
	
	template <typename F>
	bool requestCreateWrite(F _f, uint64 _sz, const size_t _flags = AllLevelFlags){
		
	}
	
	template <typename F>
	bool requestCreateWrite(F _f, uint64 _sz, AlivePointerT &_alvptr, const size_t _flags = AllLevelFlags){
		
	}
	
	template <typename F>
	bool requestRead(F _f, UidT const & _ruid, const size_t _flags = 0){
		
	}
	
	template <typename F>
	bool requestWrite(F _f, UidT const & _ruid, const size_t _flags = 0){
		
	}
	AlivePointerT alive(UidT const & _ruid);
private:
};

}//namespace temp
}//namespace frame
}//namespace solid


#endif
