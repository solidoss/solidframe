// system/device.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
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
#include "system/common.hpp"

namespace solid{

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
	int read(char	*_pb, size_t _bl);
	//! Write call
	int write(const char* _pb, size_t _bl);
	//! Cancels existing io operations
	bool cancel();
	//! Close the device
	void close();
	//! Flush the device
	void flush();
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

}//namespace solid

#endif
