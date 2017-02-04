// solid/utility/string.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_STRING_HPP
#define UTILITY_STRING_HPP

#include "solid/utility/common.hpp"
#include <cstdlib>
#include <cstring>

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

} //namespace solid

#endif