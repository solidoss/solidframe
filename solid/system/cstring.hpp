// solid/system/cstring.hpp
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
#include <string_view>

namespace solid {

const char* char_to_cstring(unsigned _c);

//! Some cross platform cstring utility functions
struct cstring {
    //! Equivalent to strcmp
    static int cmp(std::string_view _s1, std::string_view _s2) noexcept;
    //! Equivalent to strncmp
    static int ncmp(std::string_view _s1, std::string_view _s2, size_t _len) noexcept;
    //! Equivalent to strcasecmp
    static int casecmp(std::string_view _s1, std::string_view _s2) noexcept;
    //! Equivalent to strncasecmp
    static int ncasecmp(std::string_view _s1, std::string_view _s2, size_t _len) noexcept;
    //! Equivalent to strnlen - the length of a possibly not null terminated character buffer
    static size_t nlen(const char* _s, size_t _maxlen) noexcept;
    template <class T, T C1>
    static T* find(T* _pc)
    {
        while (*_pc != C1)
            ++_pc;
        return _pc;
    }

    template <class T, T C1, T C2>
    static T* find(T* _pc)
    {
        while (*_pc != C2 && *_pc != C1)
            ++_pc;
        return _pc;
    }

    template <class T, T C1, T C2, T C3>
    static T* find(T* _pc)
    {
        while (*_pc != C2 && *_pc != C1 && *_pc != C3)
            ++_pc;
        return _pc;
    }
    template <class T, T C1, T C2, T C3>
    static T* findNot(T* _pc)
    {
        while (*_pc == C2 || *_pc == C1 || *_pc == C3)
            ++_pc;
        return _pc;
    }
};

} // namespace solid
