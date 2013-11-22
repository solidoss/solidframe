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

class Store{
public:
	Store(Configuration const &_rcfg);
	
	AlivePointerT createAlive(const char* _path, const size_t _flags = 0){
		
	}
	
	WritePointerT createWrite(const char* _path, const size_t _flags = 0){
		
	}
	
	template <typename F>
	UidT openWrite(F _f, const char* _path, const size_t _flags = 0){
		
	}
	
	template <typename F>
	UidT openRead(F _f, const char* _path, const size_t _flags = 0){
		
	}
	
	template <typename F>
	bool read(F _f, UidT const & _ruid, const size_t _flags = 0){
		
	}
	
	template <typename F>
	bool write(F _f, UidT const & _ruid, const size_t _flags = 0){
		
	}
private:
};

}//namespace file
}//namespace frame
}//namespace solid


#endif
