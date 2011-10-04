/* Declarations file seekabledevice.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SYSTEM_SEEKABLE_DEVICE_HPP
#define SYSTEM_SEEKABLE_DEVICE_HPP

#include "device.hpp"

//! A wrapper for devices with random access.
class SeekableDevice: public Device{
public:
	using Device::read;
	using Device::write;
	//! Read from a given position
	int read(char *_pb, uint32 _bl, int64 _off);
	//! Write at a given position
	int write(const char *_pb, uint32 _bl, int64 _off);
	//! Move the cursor to a given position
	int64 seek(int64 _pos, SeekRef _ref = SeekBeg);
	//! Truncate to a certain length
	int truncate(int64 _len);
protected:
	SeekableDevice(int _desc = -1):Device(_desc){}
};

#endif
