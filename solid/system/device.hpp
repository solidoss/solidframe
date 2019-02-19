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

#include "solid/system/common.hpp"

namespace solid {

//! A wrapper for what on POSIX is a descriptor
class Device {
public:
#ifdef SOLID_ON_WINDOWS
    typedef void* DescriptorT;
#else
    typedef int DescriptorT;
#endif
    static constexpr DescriptorT invalidDescriptor()
    {
#ifdef SOLID_ON_WINDOWS
        return reinterpret_cast<DescriptorT>(static_cast<ptrdiff_t>(-1));
#else
        return -1;
#endif
    }
    Device(Device&& _dev) noexcept;
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
    Device& operator=(Device&& _dev) noexcept;
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
    close();
    desc = _desc;
}

} //namespace solid
