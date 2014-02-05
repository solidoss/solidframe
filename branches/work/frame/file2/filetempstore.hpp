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
	bool requestCreate(F _f, uint64 _sz, const size_t _flags = AllLevelsFlag){
		size_t	storageidx = -1;
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
	//TODO: delete when the final version is ready
	template <typename F>
	bool requestReinit(F &_f, const size_t _flags = 0){
		PointerT		uniptr;
		size_t			idx = -1;
		{
			Locker<Mutex>	lock(mutex());
			
			size_t			*pidx = NULL;
			
			if(!_f.prepare(pidx)){
				uniptr = doInsertUnique();//will use object's mutex
				*pidx = uniptr.id().first;
			}
			idx = *pidx;
		}
		return doRequestReinit(_f, idx, uniptr, _flags);//will use object's mutex
	}
	//TODO: move the final version to shared::Store
	template <typename F>
	bool requestReinit(F &_f, const size_t _flags = 0){
		
	}
	template <typename F>
	bool requestCreate(F _f, uint64 _sz, const size_t _flags = AllLevelsFlag){
		CreateCommand<F>	cmd(*this, _f, _sz);
		return requestReinit(cmd, _flags);
	}
private:
};

}//namespace temp
}//namespace file
}//namespace frame
}//namespace solid


#endif
