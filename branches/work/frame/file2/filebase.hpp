// frame/file/filebase.hpp
//
// Copyright (c) 2010 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_FILE_FILE_BASE_HPP
#define SOLID_FRAME_FILE_FILE_BASE_HPP

namespace solid{
namespace frame{
namespace file{

struct FileBase{
	virtual int read(char *_pb, uint32 _bl, int64 _off) = 0;
	virtual int write(const char *_pb, uint32 _bl, int64 _off) = 0;
};

}//namespace file
}//namespace frame
}//namespace solid

#endif
