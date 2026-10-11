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
#include <cstddef>
#include <expected>
#include <span>
#include <system_error>

namespace solid {

//! A wrapper for what on POSIX is a descriptor
class Device {
public:
#ifdef SOLID_ON_WINDOWS
    using DescriptorT = void*;
#else
    using DescriptorT = int;
#endif
    static constexpr DescriptorT invalidDescriptor();
    Device(Device&& _dev) noexcept;
    //! The constructor which will take ownership of the given descriptor
    Device(DescriptorT _desc = invalidDescriptor());
    ~Device();

    Device(const Device&)            = delete;
    Device& operator=(const Device&) = delete;

    //! Read call - returns the number of bytes read (0 on end of stream) or the system error
    [[nodiscard]] std::expected<std::size_t, std::error_code> read(std::span<std::byte> _buf);
    //! Write call - returns the number of bytes written or the system error
    [[nodiscard]] std::expected<std::size_t, std::error_code> write(std::span<const std::byte> _buf);
    //! Cancels existing io operations
    [[nodiscard]] std::expected<void, std::error_code> cancel();
    //! Close the device
    void close();
    //! Flush the device
    [[nodiscard]] std::expected<void, std::error_code> flush();
    //! Check if the device is valid
    explicit operator bool() const noexcept
    {
        return ok();
    }
    Device& operator=(Device&& _dev) noexcept;
    //! The native descriptor associated to the device
    [[nodiscard]] DescriptorT descriptor() const noexcept;

protected:
    void               descriptor(DescriptorT _desc);
    void               descriptor(DescriptorT _desc, bool);
    [[nodiscard]] bool ok() const noexcept;

private:
    DescriptorT desc_;
};

constexpr Device::DescriptorT Device::invalidDescriptor()
{
#ifdef SOLID_ON_WINDOWS
    return reinterpret_cast<DescriptorT>(static_cast<ptrdiff_t>(-1));
#else
    return -1;
#endif
}

inline bool Device::ok() const noexcept
{
    return desc_ != invalidDescriptor();
}

inline Device::DescriptorT Device::descriptor() const noexcept { return desc_; }

inline void Device::descriptor(const DescriptorT _desc)
{
    close();
    desc_ = _desc;
}

inline void Device::descriptor(const DescriptorT _desc, bool)
{
    desc_ = _desc;
}

} // namespace solid
