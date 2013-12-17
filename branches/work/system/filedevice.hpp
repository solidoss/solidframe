// system/filedevice.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_FILE_DEVICE_HPP
#define SYSTEM_FILE_DEVICE_HPP
#include <fcntl.h>
#include "seekabledevice.hpp"

namespace solid{

//! Wrapper for a file descriptor
class FileDevice: public SeekableDevice{
public:
	enum OpenMode {
		RO = O_RDONLY, //!< Read only
		WO = O_WRONLY, //!< Write only
		RW = O_RDWR, //!< Read write
		TR = O_TRUNC, //!< Truncate
		AP = O_APPEND,//!< Append
		CR = O_CREAT //!< Create
	};
	FileDevice();
	//!returns the size of a file without opening it - using stat!
	static int64 size(const char *_fname);
	//! Open a file using its name and open mode flags
	bool open(const char* _fname, int _how);
	//! Create a file using its name and open mode flags
	bool create(const char* _fname, int _how);
	//! Get the size of an opened file
	/*!
		Use FileDevice::size(const char*) to find the size of a file
		without opening it
	*/
	int64 size()const;
	//! Check if a failed open opperation may succeed on retry
	/*!
		It uses errno so, it should be used imediatly after the open call.
		It returns true in cases when the file could not be opened because
		there were no available file descriptors or kernel memory.
	*/
	bool canRetryOpen()const;
};

}//namespace solid

#endif
