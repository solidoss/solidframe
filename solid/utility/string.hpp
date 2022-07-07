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

struct CStringHash {
    size_t operator()(const char* _s) const
    {
        size_t hash = 0;

        for (; *_s; ++_s) {
            hash += *_s;
            hash += (hash << 10);
            hash ^= (hash >> 6);
        }

        hash += (hash << 3);
        hash ^= (hash >> 11);
        hash += (hash << 15);

        return hash;
    }
};

struct CStringEqual {
    bool operator()(const char* _val1, const char* _val2) const
    {
        return ::strcmp(_val1, _val2) == 0;
    }
};

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
