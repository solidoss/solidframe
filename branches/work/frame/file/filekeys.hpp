// frame/file/filekeys.hpp
//
// Copyright (c) 2010 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_FILE_FILE_KEYS_HPP
#define SOLID_FRAME_FILE_FILE_KEYS_HPP

#include "frame/file/filekey.hpp"

namespace solid{
namespace frame{
namespace file{

//! A key for requesting disk files from a NameMapper
/*!
	The file is identified through its absolute path.
	The path will be deeply copy.
	Use FastNameString to optimize the copying of the
	path, if for example the path is given as literal.
*/
struct NameKey: Key{
	NameKey(const char *_fname = NULL);
	NameKey(const std::string &_fname);
	~NameKey();
	/*virtual*/ uint32 mapperId()const;
	/*virtual*/ Key* clone() const;
	/*virtual*/ const char* path() const;
	std::string 	name;
};
//! A key like NameKey, for paths as literals
struct FastNameKey: Key{
	FastNameKey(const char *_fname = NULL);
	~FastNameKey();
	/*virtual*/ uint32 mapperId()const;
	/*virtual*/ Key* clone() const;
	/*virtual*/ const char* path() const;
private:
	const char	*name;
};

//! A key for requesting temporary files of certain capacity
struct TempKey: Key{
	TempKey(uint64 _cp = -1):cp(_cp){}
	~TempKey();
	const uint64	cp;
	/*virtual*/ uint32 mapperId()const;
	/*virtual*/ Key* clone() const;
	/*virtual*/ bool release()const;
	/*virtual*/ uint64 capacity()const;
};

//! A key for requesting memory files of certain capacity
struct MemoryKey: Key{
	MemoryKey(uint64 _cp = -1):cp(_cp){}
	~MemoryKey();
	const uint64 cp;
	/*virtual*/ uint32 mapperId()const;
	/*virtual*/ Key* clone() const;
	/*virtual*/ bool release()const;
	/*virtual*/ uint64 capacity()const;
};


}//namespace file
}//namespace frame
}//namespace solid

#endif
