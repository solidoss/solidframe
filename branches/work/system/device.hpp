/* Declarations file device.hpp
	
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

#ifndef SYSTEM_DEVICE_HPP
#define SYSTEM_DEVICE_HPP
#ifdef ON_WINDOWS
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Windows.h>

#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include "common.hpp"
//! A wrapper for what on POSIX is a descriptor
class Device{
public:
#ifdef ON_WINDOWS
	typedef HANDLE DescriptorT;
#else
	typedef int DescriptorT;
#endif
	static const DescriptorT invalidDescriptor(){
#ifdef ON_WINDOWS
		return INVALID_HANDLE_VALUE;
#else
		return -1;
#endif
	}
	//! The copy constructor which will grab the desc from the given device (like std::autoptr)
	Device(const Device &_dev);
	Device(DescriptorT _desc = invalidDescriptor());
	~Device();
	//! Read call
	int read(char	*_pb, uint32 _bl);
	//! Write call
	int write(const char* _pb, uint32 _bl);
	//! Cancels existing io operations
	bool cancel();
	//! Close the device
	void close();
	//! Flush the device
	int flush();
	//! Check if the device is valid
	bool ok()const{return desc != invalidDescriptor();}
	Device& operator=(const Device &_dev);
	//! The native descriptor associated to the socket
	DescriptorT descriptor()const;
protected:
	void descriptor(DescriptorT _desc);
private:
	mutable DescriptorT desc;
};

inline Device::DescriptorT Device::descriptor()const{return desc;}
inline void Device::descriptor(DescriptorT _desc){
	desc = _desc;
}

#endif
