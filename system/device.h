/* Declarations file device.h
	
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

#ifndef DEVICE_H
#define DEVICE_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common.h"

class Device{
public:
	Device(const Device &_dev);
	int read(char	*_pb, uint32 _bl);
	int write(const char* _pb, uint32 _bl);
	void close();
	int flush();
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
