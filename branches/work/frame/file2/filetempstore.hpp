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
namespace file{
namespace temp{

enum{
	MemoryLevelFlag = 1,
	VeryFastLevelFlag = 2,
	FastLevelFlag = 4,
	NormalLevelFlag = 8,
	SlowLevelFlag = 16,
	AllLevelsFlag = 1 + 2 + 4 + 8 + 16,
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


class Store: protected shared::Store<>{
public:
	Store(Configuration const &_rcfg);
	
	template <typename F>
	bool requestCreateAlive(F _f, uint64 _sz, const size_t _flags = AllLevelsFlag){
		size_t	storageidx;
		AsyncE	rv = findStorage(_sz, _flags, storageidx);
		
		if(rv == AsyncSuccess){
			PointerT	ptr = createFileAlive(storageidx, _sz);
			error_code	err;
			
			_f(ptr, err, true);
			return true;
		}else if(rv == AsyncWait){
			//we must wait for data to free-up within a storage
			PointerT	wptr = createFileWrite(-1, _sz);
			//PointerT	aptr = alive(wptr);
			doPushWait(wptr, _sz, _flags);
			return false;
		}else{
			//no storage can offer the requested size
			PointerT		emptyptr;
			error_code		err;
			
			_f(emptyptr, err, true);
			return true;
		}
	}
	
	template <typename F>
	bool requestCreateWrite(F _f, uint64 _sz, const size_t _flags = AllLevelFlags){
		
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
}//namespace file
}//namespace frame
}//namespace solid


#endif
