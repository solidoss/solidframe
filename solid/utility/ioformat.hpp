// solid/utility/ioforamt.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_IOFORMAT_HPP
#define UTILITY_IOFORMAT_HPP

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

} //namespace solid

#endif
