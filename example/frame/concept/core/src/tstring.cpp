// tstring.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "core/tstring.hpp"
#include <cstring>
namespace solid{
void append(String &_str, solid::ulong _v){
    if(!_v){
        _str+='0';
    }else{
        char tmp[12];
        ushort pos = 11;
        while(_v){
            *(tmp+pos)='0'+_v%10;
            _v/=10;
            --pos;
        }
        ++pos;
        _str.append(tmp+pos,12 - pos);
    }
}
}//namespace solid
