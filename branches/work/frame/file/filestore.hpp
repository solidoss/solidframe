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

namespace solid{
namespace frame{
namespace file{

struct Configuration{
	struct Storage{
		std::string		localpath;
		std::string		pathprefix;
	};
	
	std::vector<Storage>	StorageVectorT;
	
	StorageVectorT		storagevec;
};

typedef shared::AlivePointer	AlivePointerT;
typedef shared::WritePointer<>	WritePointerT;
typedef shared::ReadPointer<>	ReadPointerT;

/*
 * NOTE: all request methods return true if _f(...) was called synchronously
 * 
 * F signature should be _f(PointerT &_ptr, error_code _err, bool _synchronous)
 */
class Store{
public:
	Store(Configuration const &_rcfg);
	//If a file with _path already exists in the store, the call will be similar with open with truncate openflag
	template <typename F>
	bool requestCreateAlive(F _f, const char* _path, const size_t _openflags = 0, const size_t _flags = 0){
		
	}
	template <typename F>
	bool requestCreateAlive(F _f, const char* _path, AlivePointerT &_alvptr, const size_t _openflags = 0, const size_t _flags = 0){
		
	}
	template <typename F>
	bool requestCreateWrite(F _f, const char* _path, const size_t _openflags = 0, const size_t _flags = 0){
		
	}
	
	template <typename F>
	bool requestCreateWrite(F _f, const char* _path, AlivePointerT &_alvptr, const size_t _openflags = 0, const size_t _flags = 0){
		
	}
	
	template <typename F>
	bool requestOpenWrite(F _f, const char* _path, const size_t _openflags = 0, const size_t _flags = 0){
		
	}
	
	template <typename F>
	bool requestOpenWrite(F _f, const char* _path, AlivePointerT &_alvptr, const size_t _openflags = 0, const size_t _flags = 0){
		
	}
	
	template <typename F>
	bool requestOpenRead(F _f, const char* _path, const size_t _openflags = 0, const size_t _flags = 0){
		
	}
	
	template <typename F>
	bool requestOpenRead(F _f, const char* _path, AlivePointerT &_alvptr, const size_t _openflags = 0, const size_t _flags = 0){
		
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

}//namespace file
}//namespace frame
}//namespace solid


#endif
