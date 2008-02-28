/* Declarations file device.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DEVICE_H
#define DEVICE_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common.hpp"
//! A wrapper for what on POSIX is a descriptor
class Device{
public:
	//! The copy constructor which will grab the desc from the given device (like std::autoptr)
	Device(const Device &_dev);
	//! Read call
	int read(char	*_pb, uint32 _bl);
	//! Write call
	int write(const char* _pb, uint32 _bl);
	//! Close the device
	void close();
	//! Flush the device
	int flush();
	//! Check if the device is valid
	bool ok()const{return desc >= 0;}
	Device& operator&(Device &_dev);
protected:
	Device(int _desc = -1);
	~Device();
	int descriptor()const;
	void descriptor(int _desc);
private:
	mutable int	desc;
};

inline int Device::descriptor()const{return desc;}
inline void Device::descriptor(int _desc){
	desc = _desc;
}

#endif
