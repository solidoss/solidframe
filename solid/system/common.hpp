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

#include "solid/system/configuration_impl.hpp"
#include "solid/system/version.hpp"
#include <cstdint>
#include <cstdlib>
#include <type_traits>

namespace solid {

using uchar     = unsigned char;
using uint      = unsigned int;
using ulong     = unsigned long;
using ushort    = unsigned short;
using longlong  = long long;
using ulonglong = unsigned long long;

enum SeekRef {
    SeekBeg = 0,
    SeekCur = 1,
    SeekEnd = 2
};

struct EmptyType {
};

class NullType {
};

template <class T>
struct TypeToType {
    using TypeT = T;
};

class NonCopyable {
protected:
    NonCopyable() {}
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&)      = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable& operator=(NonCopyable&&) = delete;
};

using ssize_t = std::make_signed<size_t>::type;

} //namespace solid

//Some macro helpers:

//adapted from: https://stackoverflow.com/questions/9183993/msvc-variadic-macro-expansion/9338429#9338429
#define SOLID_GLUE(x, y) x y

#define SOLID_RETURN_ARG_COUNT(_1_, _2_, _3_, _4_, _5_, count, ...) count
#define SOLID_EXPAND_ARGS(args) SOLID_RETURN_ARG_COUNT args
#define SOLID_COUNT_ARGS_MAX5(...) SOLID_EXPAND_ARGS((__VA_ARGS__, 5, 4, 3, 2, 1, 0))

#define SOLID_OVERLOAD_MACRO2(name, count) name##count
#define SOLID_OVERLOAD_MACRO1(name, count) SOLID_OVERLOAD_MACRO2(name, count)
#define SOLID_OVERLOAD_MACRO(name, count) SOLID_OVERLOAD_MACRO1(name, count)

#define SOLID_CALL_OVERLOAD(name, ...) SOLID_GLUE(SOLID_OVERLOAD_MACRO(name, SOLID_COUNT_ARGS_MAX5(__VA_ARGS__)), (__VA_ARGS__))
