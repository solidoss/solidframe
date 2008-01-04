/* Declarations file seekabledevice.h
	
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

#ifndef SEEKABLE_DEVICE_H
#define SEEKABLE_DEVICE_H

#include "device.h"

class SeekableDevice: public Device{
public:
	int read(char *_pb, uint32 _bl, int64 _off);
	int write(const char *_pb, uint32 _bl, int64 _off);
	int read(char *_pb, uint32 _bl){
		return Device::read(_pb, _bl);
	}
	int write(const char *_pb, uint32 _bl){
		return Device::write(_pb, _bl);
	}
	int64 seek(int64 _pos, SeekRef _ref = SeekBeg);
	int truncate(int64 _len);
protected:
	SeekableDevice(int _desc = -1):Device(_desc){}
};

#endif
