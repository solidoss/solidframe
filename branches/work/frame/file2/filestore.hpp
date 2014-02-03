// frame/file/filestore.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_FILE_STORE_HPP
#define SOLID_FRAME_FILE_STORE_HPP

#include "frame/sharedstore.hpp"
#include "frame/file2/filebase.hpp"
#include "system/filedevice.hpp"

namespace solid{
namespace frame{
namespace file{

struct Configuration{
	struct Storage{
		std::string		localpath;
		std::string		pathprefix;
	};
	
	typedef std::vector<Storage>	StorageVectorT;
	
	StorageVectorT		storagevec;
};

struct File: FileBase{
	void clear();
	int open(const char *_path, const size_t _openflags);
	/*virtual*/ int read(char *_pb, uint32 _bl, int64 _off);
	/*virtual*/ int write(const char *_pb, uint32 _bl, int64 _off);
	int64 size()const;
	int truncate(int64 _len = 0);
private:
	FileDevice	fd;
};

class Store: public shared::Store<File>{
public:
	Store(Configuration const &_rcfg);
	//If a file with _path already exists in the store, the call will be similar with open with truncate openflag
	template <typename F>
	bool requestCreateAlive(F _f, const char* _path, const size_t _openflags = 0, const size_t _flags = 0){
		Locker<Mutex>	lock(mutex());
		
		OpenCommand<F>	opencmd(_f, _openflags);
		size_t			idx;
		bool			exists = doInsertFilePath(_path, opencmd.path, idx);
		
		PointerT		alvptr = doUnsafeInsertAlive();
		
		if(fs.empty()){
			//no one is waiting (not even an AlivePointer) - the use count is 0 - the file stub was just created.
			error_code err = fs.open(_openflags);
			_f(AlivePointerT(fs), err);
			return true;
		}else{
			
			insertCallback(idx, opencmd, _flags);
			return false;
		}
	}
	
	template <typename F>
	bool requestCreateWrite(F _f, const char* _path, const size_t _openflags = 0, const size_t _flags = 0){
		
	}
	
	template <typename F>
	bool requestOpenWrite(F _f, const char* _path, const size_t _openflags = 0, const size_t _flags = 0){
		
	}
	
	template <typename F>
	bool requestOpenRead(F _f, const char* _path, const size_t _openflags = 0, const size_t _flags = 0){
		
	}
	
	template <typename F>
	bool requestRead(F _f, UidT const & _ruid, const size_t _flags = 0){
		
	}
	
	template <typename F>
	bool requestWrite(F _f, UidT const & _ruid, const size_t _flags = 0){
		
	}
	AlivePointerT alive(UidT const & _ruid);
private:
	
private:
	struct Data;
	Data	&d;
};

}//namespace file
}//namespace frame
}//namespace solid


#endif
