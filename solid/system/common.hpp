// solid/system/common.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/solid_config.hpp"
#include <cstdint>
#include <cstdlib>

#ifdef SOLID_ON_WINDOWS
#define UBOOSTMUTEX
#define UBOOSTSHAREDPTR
//#endif
#endif

namespace solid {

typedef unsigned char uchar;
typedef unsigned int  uint;

typedef unsigned long  ulong;
typedef unsigned short ushort;

typedef long long          longlong;
typedef unsigned long long ulonglong;

enum SeekRef {
    SeekBeg = 0,
    SeekCur = 1,
    SeekEnd = 2
};

struct EmptyType {
};
class NullType {
};

template <size_t V>
struct SizeToType {
    enum { value = V };
};

template <bool V>
struct BoolToType {
    enum { value = V };
};

template <class T>
struct TypeToType {
    using TypeT = T;
};

template <class T>
struct UnsignedType;

template <>
struct UnsignedType<int8_t> {
    typedef uint8_t Type;
};

template <>
struct UnsignedType<int16_t> {
    typedef uint16_t Type;
};

template <>
struct UnsignedType<int32_t> {
    typedef uint32_t Type;
};

template <>
struct UnsignedType<int64_t> {
    typedef uint64_t Type;
};

template <>
struct UnsignedType<uint8_t> {
    typedef uint8_t Type;
};

template <>
struct UnsignedType<uint16_t> {
    typedef uint16_t Type;
};

template <>
struct UnsignedType<uint32_t> {
    typedef uint32_t Type;
};

template <>
struct UnsignedType<uint64_t> {
    typedef uint64_t Type;
};

const char* src_file_name(char const* _fname);

} //namespace solid
