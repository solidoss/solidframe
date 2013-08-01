// frame/file/filemappers.hpp
//
// Copyright (c) 2010 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_FILE_FILE_MAPPERS_HPP
#define SOLID_FRAME_FILE_FILE_MAPPERS_HPP

#include "frame/file/filemanager.hpp"
#include "frame/file/filemapper.hpp"

namespace solid{
namespace frame{
namespace file{

//! A mapper for disk files
struct NameMapper: Mapper{
	NameMapper(uint32 _cnt = -1, uint32 _wait = 0);
	/*virtual*/ ~NameMapper();
	/*virtual*/ void id(uint32 _id);
	/*virtual*/ void execute(Manager::Stub &_rs);
	/*virtual*/ void stop();
	/*virtual*/ bool erase(File *_pf);
	/*virtual*/ File* findOrCreateFile(const Key &_rk);
	/*virtual*/ int open(File &_rf);
	/*virtual*/ void close(File &_rf);
	/*virtual*/ bool getTimeout(TimeSpec &_rts);
private:
	struct Data;
	Data &d;
};

//! A mapper for temporary files
struct TempMapper: Mapper{
	//! Constructor
	/*!
		You must use a TempKey to request a file from
		TempMapper. You will need to specify the maximum size of
		the requested temp file.
		\param _cp The total capacity for all open temp files
		\param _pfx Path to temp directory
	*/
	TempMapper(uint64 _cp, const char *_pfx);
	/*virtual*/ ~TempMapper();
	/*virtual*/ void id(uint32 _id);
	/*virtual*/ void execute(Manager::Stub &_rs);
	/*virtual*/ void stop();
	/*virtual*/ bool erase(File *_pf);
	/*virtual*/ File* findOrCreateFile(const Key &_rk);
	/*virtual*/ int open(File &_rf);
	/*virtual*/ void close(File &_rf);
	/*virtual*/ bool getTimeout(TimeSpec &_rts);
private:
	struct Data;
	Data &d;
};

//! A mapper for in Memory files
struct MemoryMapper: Mapper{
	//! Constructor
	/*!
		You must use a MemoryKey to request a file from
		MemoryMapper. You will need to specify the maximum size of
		the requested memory file.
		\param _cp The total capacity for all open memory files
	*/
	MemoryMapper(uint64 _cp);
	/*virtual*/ ~MemoryMapper();
	/*virtual*/ void id(uint32 _id);
	/*virtual*/ void execute(Manager::Stub &_rs);
	/*virtual*/ void stop();
	/*virtual*/ bool erase(File *_pf);
	/*virtual*/ File* findOrCreateFile(const Key &_rk);
	/*virtual*/ int open(File &_rf);
	/*virtual*/ void close(File &_rf);
	/*virtual*/ bool getTimeout(TimeSpec &_rts);
private:
	struct Data;
	Data &d;
};

}//namespace file
}//namespace frame
}//namespace solid


#endif
