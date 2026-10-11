// solid/system/seekabledevice.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/device.hpp"
#include <cstdint>

namespace solid {

//! A wrapper for devices with random access.
class SeekableDevice : public Device {
public:
    using Device::read;
    using Device::write;
    //! Read from a given position - returns the number of bytes read or the system error
    [[nodiscard]] std::expected<std::size_t, std::error_code> read(std::span<std::byte> _buf, int64_t _off);
    //! Write at a given position - returns the number of bytes written or the system error
    [[nodiscard]] std::expected<std::size_t, std::error_code> write(std::span<const std::byte> _buf, int64_t _off);
    //! Move the cursor to a given position - returns the resulting offset from the beginning
    [[nodiscard]] std::expected<int64_t, std::error_code> seek(int64_t _pos, SeekRef _ref = SeekBeg);
    //! Truncate to a certain length
    [[nodiscard]] std::expected<void, std::error_code> truncate(int64_t _len);

protected:
    SeekableDevice(DescriptorT _desc = invalidDescriptor())
        : Device(_desc)
    {
    }
};

} // namespace solid
