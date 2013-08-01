// frame/file/filekey.hpp
//
// Copyright (c) 2010 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_FILE_FILE_KEY_HPP
#define SOLID_FRAME_FILE_FILE_KEY_HPP

#include "system/common.hpp"
#include <string>

namespace solid{
namespace frame{
namespace file{
//! A key for requsting a file from file::Manager
struct Key{
	virtual ~Key();
	virtual uint32 mapperId()const = 0;
	virtual Key* clone() const = 0;
	//! If returns true the file key will be deleted
	virtual bool release()const;
	virtual const char* path() const;
	virtual uint64 capacity()const;
};

}//namespace file
}//namepsace frame
}//namespace solid
#endif
