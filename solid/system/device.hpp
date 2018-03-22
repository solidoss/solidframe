// solid/system/device.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once
#ifdef SOLID_ON_WINDOWS
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>

#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include "solid/system/common.hpp"

#ifdef SOLID_ON_WINDOWS

using ssize_t = long;

#endif

namespace solid {

//! A wrapper for what on POSIX is a descriptor
class Device {
public:
#ifdef SOLID_ON_WINDOWS
    typedef HANDLE DescriptorT;
	using ssize_t = long;
#else
    typedef int DescriptorT;
#endif
    static DescriptorT invalidDescriptor()
    {
#ifdef SOLID_ON_WINDOWS
        return INVALID_HANDLE_VALUE;
#else
        return -1;
#endif
    }
    Device(Device&& _dev);
    //! The copy constructor which will grab the desc from the given device (like std::autoptr)
    Device(DescriptorT _desc = invalidDescriptor());
    ~Device();
    //! Read call
    ssize_t read(char* _pb, size_t _bl);
    //! Write call
    ssize_t write(const char* _pb, size_t _bl);
    //! Cancels existing io operations
    bool cancel();
    //! Close the device
    void close();
    //! Flush the device
    void flush();
    //! Check if the device is valid
    explicit operator bool() const noexcept
    {
        return ok();
    }
    Device& operator=(Device&& _dev);
    //! The native descriptor associated to the socket
    DescriptorT descriptor() const;

protected:
    void descriptor(DescriptorT _desc);
    bool ok() const noexcept
    {
        return desc != invalidDescriptor();
    }

private:
    Device(const Device& _dev);
    Device&     operator=(const Device& _dev);
    DescriptorT desc;
};

inline Device::DescriptorT Device::descriptor() const { return desc; }
inline void                Device::descriptor(DescriptorT _desc)
{
    desc = _desc;
}

} //namespace solid
