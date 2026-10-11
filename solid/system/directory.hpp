// solid/system/directory.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <expected>
#include <string_view>
#include <system_error>

namespace solid {

//! A wrapper for filesystem directory operations
class Directory {
public:
    //! Create a new directory
    [[nodiscard]] static std::expected<void, std::error_code> create(std::string_view _path);
    //! Create a new path of directories - an already existing directory is not an error
    [[nodiscard]] static std::expected<void, std::error_code> create_all(std::string_view _path);
    //! Erase a file
    [[nodiscard]] static std::expected<void, std::error_code> erase_file(std::string_view _path);
    //! Rename a file
    [[nodiscard]] static std::expected<void, std::error_code> rename_file(std::string_view _from, std::string_view _to);
};

} // namespace solid
