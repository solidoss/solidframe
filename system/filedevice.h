/* Declarations file filedevice.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FILE_DEVICE_H
#define FILE_DEVICE_H
#include <fcntl.h>
#include "seekabledevice.h"
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
	int open(const char* _fname, int _how);
	//! Create a file using its name and open mode flags
	int create(const char* _fname, int _how);
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

#endif
