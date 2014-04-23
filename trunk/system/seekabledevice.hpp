// system/seekabledevice.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_SEEKABLE_DEVICE_HPP
#define SYSTEM_SEEKABLE_DEVICE_HPP

#include "device.hpp"

namespace solid{

//! A wrapper for devices with random access.
class SeekableDevice: public Device{
public:
	using Device::read;
	using Device::write;
	//! Read from a given position
	int read(char *_pb, size_t _bl, int64 _off);
	//! Write at a given position
	int write(const char *_pb, size_t _bl, int64 _off);
	//! Move the cursor to a given position
	int64 seek(int64 _pos, SeekRef _ref = SeekBeg);
	//! Truncate to a certain length
	bool truncate(int64 _len);
protected:
	SeekableDevice(DescriptorT _desc = invalidDescriptor()):Device(_desc){}
};

}//namespace solid

#endif
