// solid/system/convertors.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "common.hpp"

namespace solid {
//!A template convertor to an unsigned
/*!
    it convert a basic type to it unsigned equivalent:<br>
    int -> unsigned int<br>
    long -> unsigned long<br>
    usigned long -> unsigned long<br>
    etc.<br>
*/
template <class T>
struct UnsignedConvertor;

template <>
struct UnsignedConvertor<int16_t> {
    typedef uint16_t UnsignedType;
};

template <>
struct UnsignedConvertor<int> {
    typedef unsigned int UnsignedType;
};

template <>
struct UnsignedConvertor<long> {
    typedef ulong UnsignedType;
};

template <>
struct UnsignedConvertor<long long> {
    typedef unsigned long long UnsignedType;
};

template <>
struct UnsignedConvertor<uint16_t> {
    typedef uint16_t UnsignedType;
};

template <>
struct UnsignedConvertor<unsigned int> {
    typedef unsigned int UnsignedType;
};

template <>
struct UnsignedConvertor<unsigned long> {
    typedef unsigned long UnsignedType;
};

template <>
struct UnsignedConvertor<unsigned long long> {
    typedef unsigned long long UnsignedType;
};

constexpr inline uint32_t bits_to_mask32(unsigned v)
{
    return (1UL << v) - 1;
}
constexpr inline uint32_t bits_to_count32(unsigned v)
{
    return (1UL << v) & (~static_cast<uint32_t>(1));
}

constexpr inline uint64_t bits_to_mask64(unsigned v)
{
    return (1ULL << v) - 1;
}

constexpr inline uint64_t bits_to_count64(unsigned v)
{
    return (1ULL << v) & (~static_cast<uint64_t>(1));
}

constexpr inline size_t bits_to_mask(const size_t v)
{
    return (static_cast<size_t>(1) << v) - 1;
}

constexpr inline size_t bits_to_count(const size_t v)
{
    return (1ULL << v) & (~static_cast<size_t>(1));
}

#ifndef SOLID_ON_WINDOWS
constexpr
#endif
    inline uint16_t
    swap_bytes(const uint16_t _v)
{
#ifdef SOLID_ON_WINDOWS
    return _byteswap_ushort(_v);
#else
    return ((((_v) >> 8) & 0xff) | (((_v)&0xff) << 8));
#endif
}

#ifndef SOLID_ON_WINDOWS
constexpr
#endif
    inline uint32_t
    swap_bytes(const uint32_t _v)
{
#ifdef SOLID_ON_WINDOWS
    return _byteswap_ulong(_v);
#elif defined(SOLID_USE_GCC_BSWAP)
    return __builtin_bswap32(_v);
#else
    // clang-format off
    return (
        (((_v)&0xff000000) >> 24) |
        (((_v)&0x00ff0000) >> 8)  |
        (((_v)&0x0000ff00) << 8)  |
        (((_v)&0x000000ff) << 24)
    );
    // clang-format on
#endif
}

#ifndef SOLID_ON_WINDOWS
constexpr
#endif
    inline uint64_t
    swap_bytes(const uint64_t _v)
{
#ifdef SOLID_ON_WINDOWS
    return _byteswap_uint64(_v);
#elif defined(SOLID_USE_GCC_BSWAP)
    return __builtin_bswap64(_v);
#else
    // clang-format off
    return (
        (((_v)&0xff00000000000000ull) >> 56) |
        (((_v)&0x00ff000000000000ull) >> 40) |
        (((_v)&0x0000ff0000000000ull) >> 24) |
        (((_v)&0x000000ff00000000ull) >> 8)  |
        (((_v)&0x00000000ff000000ull) << 8)  |
        (((_v)&0x0000000000ff0000ull) << 24) |
        (((_v)&0x000000000000ff00ull) << 40) |
        (((_v)&0x00000000000000ffull) << 56)
    );
    // clang-format on
#endif
}

} //namespace solid
