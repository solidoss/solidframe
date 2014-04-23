// frame/file/tempbase.hpp
//
// Copyright (c) 2010 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_FILE_TEMP_BASE_HPP
#define SOLID_FRAME_FILE_TEMP_BASE_HPP

#include "system/common.hpp"
#include "system/error.hpp"

namespace solid{
namespace frame{
namespace file{

struct TempBase{
	const size_t	tempstorageid;
	const size_t	tempid;
	const uint64	tempsize;
	
	TempBase(
		size_t _storageid,
		size_t _id,
		uint64 _size
	):tempstorageid(_storageid), tempid(_id), tempsize(_size){}
	
	virtual ~TempBase();
	
	virtual bool open(
		const char *_path,
		const size_t _openflags,
		bool _remove,
		ERROR_NS::error_code &_rerr
	) = 0;
	
	virtual void close(const char *_path, bool _remove) = 0;
	
	virtual int read(char *_pb, uint32 _bl, int64 _off) = 0;
	virtual int write(const char *_pb, uint32 _bl, int64 _off) = 0;
	virtual int64 size()const = 0;
	virtual bool truncate(int64 _len = 0) = 0;
	virtual void flush();
};

}//namespace file
}//namespace frame
}//namespace solid

#endif
