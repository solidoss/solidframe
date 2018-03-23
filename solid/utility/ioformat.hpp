// solid/utility/ioforamt.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <ostream>
#include <string>

namespace solid {

struct TrimString {
    const char* pstr;
    size_t      strlen;
    size_t      beglen;
    size_t      endlen;

    TrimString(
        const char* _pstr,
        size_t      _strlen,
        size_t      _beglen,
        size_t      _endlen)
        : pstr(_pstr)
        , strlen(_strlen)
        , beglen(_beglen)
        , endlen(_endlen)
    {
    }
};

inline TrimString trim_str(std::string const& _rstr, size_t _beglen, size_t _endlen)
{
    return TrimString(_rstr.c_str(), _rstr.size(), _beglen, _endlen);
}

inline TrimString trim_str(const char* _pcstr, size_t _strlen, size_t _beglen, size_t _endlen)
{
    return TrimString(_pcstr, _strlen, _beglen, _endlen);
}

std::ostream& operator<<(std::ostream& _ros, TrimString const&);

template <size_t Sz>
struct Format;

template <>
struct Format<0> {
	char		buf[1];
	std::string str;
};

template <size_t Sz>
struct Format {
    char        buf[Sz];
    std::string str;
};

template <size_t Sz, typename... Args>
inline Format<Sz> format(const char* _fmt, Args... args)
{
    Format<Sz>   fmt;
    const size_t sz = snprintf(nullptr, 0, _fmt, args...) + 1;
    if (sz <= Sz) {
        snprintf(fmt.buf, Sz, _fmt, args...);
    } else {
        fmt.str.resize(sz + 1);
        snprintf(const_cast<char*>(fmt.str.data()), sz, _fmt, args...);
        fmt.str.resize(sz - 1);
    }
    return fmt;
}

template <size_t Sz>
std::ostream& operator<<(std::ostream& _ros, Format<Sz> const& _rfmt)
{
    if (_rfmt.str.empty()) {
        _ros << _rfmt.buf;
    } else {
        _ros << _rfmt.str;
    }
    return _ros;
}

} //namespace solid
