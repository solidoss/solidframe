// solid/serialization/v2/binarybasic.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/utility/algorithm.hpp"
#include <array>
#include <cstring>

namespace solid {
namespace serialization {
namespace v2 {
namespace binary {

inline char* store(char* _pd, const uint8_t _val)
{
    uint8_t* pd = reinterpret_cast<uint8_t*>(_pd);
    *pd         = _val;
    return _pd + 1;
}

inline char* store(char* _pd, const uint16_t _val)
{
    uint8_t* pd = reinterpret_cast<uint8_t*>(_pd);
    *(pd)       = ((_val >> 8) & 0xff);
    *(pd + 1)   = (_val & 0xff);
    return _pd + 2;
}

inline char* store(char* _pd, const uint32_t _val)
{

    _pd = store(_pd, static_cast<uint16_t>(_val >> 16));

    return store(_pd, static_cast<uint16_t>(_val & 0xffff));
    ;
}

inline char* store(char* _pd, const uint64_t _val)
{

    _pd = store(_pd, static_cast<uint32_t>(_val >> 32));

    return store(_pd, static_cast<uint32_t>(_val & 0xffffffffULL));
    ;
}

template <size_t S>
inline char* store(char* _pd, const std::array<uint8_t, S> _val)
{
    memcpy(_pd, _val.data(), S);
    return _pd + S;
}

inline const char* load(const char* _ps, uint8_t& _val)
{
    const uint8_t* ps = reinterpret_cast<const uint8_t*>(_ps);
    _val              = *ps;
    return _ps + 1;
}

inline const char* load(const char* _ps, uint16_t& _val)
{
    const uint8_t* ps = reinterpret_cast<const uint8_t*>(_ps);
    _val              = *ps;
    _val <<= 8;
    _val |= *(ps + 1);
    return _ps + 2;
}

inline const char* load(const char* _ps, uint32_t& _val)
{
    uint16_t upper;
    uint16_t lower;
    _ps  = load(_ps, upper);
    _ps  = load(_ps, lower);
    _val = upper;
    _val <<= 16;
    _val |= lower;
    return _ps;
}

inline const char* load(const char* _ps, uint64_t& _val)
{
    uint32_t upper;
    uint32_t lower;
    _ps  = load(_ps, upper);
    _ps  = load(_ps, lower);
    _val = upper;
    _val <<= 32;
    _val |= lower;
    return _ps;
}
template <size_t S>
inline const char* load(const char* _ps, std::array<uint8_t, S>& _val)
{
    memcpy(_val.data, _ps, S);
    return _ps + S;
}

//cross integer serialization
namespace cross {
inline size_t size(const char* _ps)
{
    const uint8_t* ps = reinterpret_cast<const uint8_t*>(_ps);
    uint8_t        v  = *ps;
    const bool     ok = check_value_with_crc(v, v);
    if (ok) {
        return v + 1;
    }
    return InvalidSize();
}

inline size_t size(uint8_t _v)
{
    return max_padded_byte_cout(_v) + 1;
}

inline size_t size(uint16_t _v)
{
    return max_padded_byte_cout(_v) + 1;
}

inline size_t size(uint32_t _v)
{
    return max_padded_byte_cout(_v) + 1;
}
inline size_t size(uint64_t _v)
{
    return max_padded_byte_cout(_v) + 1;
}

char* store(char* _pd, const size_t _sz, uint8_t _v);
char* store(char* _pd, const size_t _sz, uint16_t _v);
char* store(char* _pd, const size_t _sz, uint32_t _v);
char* store(char* _pd, const size_t _sz, uint64_t _v);

const char* load(const char* _ps, const size_t _sz, uint8_t& _val);
const char* load(const char* _ps, const size_t _sz, uint16_t& _val);
const char* load(const char* _ps, const size_t _sz, uint32_t& _val);
const char* load(const char* _ps, const size_t _sz, uint64_t& _val);
} //namespace cross
} //namespace binary
} //namespace v2
} //namespace serialization
} //namespace solid
