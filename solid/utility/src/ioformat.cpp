// solid/utility/src/ioformat.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/utility/ioformat.hpp"

namespace solid {

std::ostream& operator<<(std::ostream& _ros, TrimString const& _trimstr)
{
    size_t beglen = _trimstr.beglen;
    size_t endlen = _trimstr.endlen;

    if ((beglen + endlen) >= _trimstr.strlen) {
        beglen = _trimstr.strlen;
        endlen = 0;
    } else if (beglen < _trimstr.strlen) {
        endlen = _trimstr.strlen - beglen;
        if (endlen > _trimstr.endlen) {
            endlen = _trimstr.endlen;
        }
    }

    _ros.write(_trimstr.pstr, beglen);
    if (endlen != 0 or beglen < _trimstr.strlen) {
        _ros << '.' << '.' << '.';
    }
    _ros.write(_trimstr.pstr + _trimstr.strlen - endlen, endlen);
    return _ros;
}

} //namespace solid
