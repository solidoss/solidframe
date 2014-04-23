// frame/file/filetemp.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_FILE_TEMP_HPP
#define SOLID_FRAME_FILE_TEMP_HPP

#include "system/common.hpp"
#include "system/filedevice.hpp"

#include "frame/file/tempbase.hpp"
#include "utility/memoryfile.hpp"


namespace solid{
namespace frame{
namespace file{

struct TempFile: TempBase{
	TempFile(
		size_t _storageid,
		size_t _id,
		uint64 _size
	);//:TempBase(_storageid, _id, _size){
private:	
	/*virtual*/ ~TempFile();
	
	/*virtual*/ bool open(const char *_path, const size_t _openflags, bool _remove, ERROR_NS::error_code &_rerr);
	/*virtual*/ void close(const char *_path, bool _remove);
	/*virtual*/ int read(char *_pb, uint32 _bl, int64 _off);
	/*virtual*/ int write(const char *_pb, uint32 _bl, int64 _off);
	/*virtual*/ int64 size()const;
	
	/*virtual*/ bool truncate(int64 _len = 0);
	/*virtual*/ void flush();
private:
	FileDevice	fd;
};

struct TempMemory: TempBase{
	TempMemory(
		size_t _storageid,
		size_t _id,
		uint64 _size
	);//:TempBase(_storageid, _id, _size){
private:	
	/*virtual*/ ~TempMemory();
	
	/*virtual*/ bool open(const char *_path, const size_t _openflags, bool _remove, ERROR_NS::error_code &_rerr);
	/*virtual*/ void close(const char *_path, bool _remove);
	/*virtual*/ int read(char *_pb, uint32 _bl, int64 _off);
	/*virtual*/ int write(const char *_pb, uint32 _bl, int64 _off);
	/*virtual*/ int64 size()const;
	
	/*virtual*/ bool truncate(int64 _len = 0);
private:
	MemoryFile	mf;
};


}//namespace file
}//namespace frame
}//namespace solid

#endif
