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

class FileDevice: public SeekableDevice{
public:
	enum OpenMode {
		RO = O_RDONLY, 
		WO = O_WRONLY, 
		RW = O_RDWR,
		TR = O_TRUNC,
		AP = O_APPEND,
		CR = O_CREAT
	};
	FileDevice();
	int open(const char* _fname, int _how);
	int create(const char* _fname, int _how);
	int64 size()const;
};

#endif
