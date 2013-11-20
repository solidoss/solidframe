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

class Store: protected shared::Store<>{
public:
	Store(Configuration const &_rcfg);
	
	template <typename F>
	void createAlive(F _f, uint64 _sz, const size_t _flags = AllLevelFlags){
		
	}
	
	template <typename F>
	void createWrite(F _f, uint64 _sz, const size_t _flags = AllLevelFlags){
		
	}
	
	template <typename F>
	void read(F _f, UidT const & _ruid, const size_t _flags = 0){
		
	}
	
	template <typename F>
	void write(F _f, UidT const & _ruid, const size_t _flags = 0){
		
	}
	
private:
};

}//namespace temp
}//namespace frame
}//namespace solid


#endif
