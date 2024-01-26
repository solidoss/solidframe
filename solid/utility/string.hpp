// solid/utility/string.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/utility/common.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>

namespace solid {

inline void to_lower(std::string& _rstr)
{
    std::transform(_rstr.begin(), _rstr.end(), _rstr.begin(), [](unsigned char c) { return std::tolower(c); });
}

inline void to_upper(std::string& _rstr)
{
    std::transform(_rstr.begin(), _rstr.end(), _rstr.begin(), [](unsigned char c) { return std::toupper(c); });
}

uint64_t make_number(std::string _str);

} // namespace solid
