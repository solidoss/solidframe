// solid/system/filedevice.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once
#include "solid/system/seekabledevice.hpp"
#include <fcntl.h>
#include <string_view>

namespace solid {

//! Wrapper for a file descriptor
class FileDevice : public SeekableDevice {
public:
    enum OpenMode {
        ReadOnlyE  = O_RDONLY, //!< Read only
        WriteOnlyE = O_WRONLY, //!< Write only
        ReadWriteE = O_RDWR, //!< Read write
        TruncateE  = O_TRUNC, //!< Truncate
        AppendE    = O_APPEND, //!< Append
        CreateE    = O_CREAT //!< Create
    };
    FileDevice();
    //! Returns the size of a file without opening it - using stat!
    [[nodiscard]] static std::expected<int64_t, std::error_code> size(std::string_view _fname);
    //! Open a file using its name and open mode flags
    [[nodiscard]] std::expected<void, std::error_code> open(std::string_view _fname, int _how);
    //! Create a file using its name and open mode flags
    [[nodiscard]] std::expected<void, std::error_code> create(std::string_view _fname, int _how);
    //! Get the size of an opened file
    /*!
        Use FileDevice::size(std::string_view) to find the size of a file
        without opening it
    */
    [[nodiscard]] std::expected<int64_t, std::error_code> size() const;
    //! Check if a failed open operation may succeed on retry
    /*!
        It returns true in cases when the file could not be opened because
        there were no available file descriptors or kernel memory.
    */
    [[nodiscard]] static bool can_retry_open(const std::error_code& _err) noexcept;
};

} // namespace solid
