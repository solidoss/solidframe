// solid/system/exception.hpp
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
#include "solid/system/debug.hpp"
#include "solid/system/error.hpp"
#include <exception>
#include <iosfwd>
#include <sstream>
#include <string>

// struct solid_oss_str {
// };
// 
// inline std::string operator<<(std::ostream& _ros, const solid_oss_str&)
// {
//     return static_cast<std::ostringstream&>(_ros).str();
// }

namespace solid {

#ifndef CRT_FUNCTION_NAME
#ifdef SOLID_ON_WINDOWS
#define CRT_FUNCTION_NAME __func__
#else
#define CRT_FUNCTION_NAME __FUNCTION__
#endif
#endif

struct LogicError: std::logic_error{
    template <typename F>
    LogicError(const F &_rf, const char *_file, int _line, const char *_fnc):std::logic_error(_rf(_file, _line, _fnc)){}
    
};

#define SOLID_THROW(x) \
    throw solid::LogicError(\
        [&](const char* _file, int _line, const char *_fnc){\
            std::ostringstream os; os<<'['<<_file <<'('<<_line<< ")][" << _fnc << "] " << x; return os.str();\
        }, __FILE__, __LINE__, CRT_FUNCTION_NAME\
    )


//adapted from https://github.com/Microsoft/GSL/blob/master/include/gsl/gsl_assert
#if defined(__clang__) || defined(__GNUC__)
#define SOLID_LIKELY(x) __builtin_expect(!!(x), 1)
#define SOLID_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define SOLID_LIKELY(x) (!!(x))
#define SOLID_UNLIKELY(x) (!!(x))
#endif

#define SOLID_CHECK1(a) \
    (SOLID_LIKELY(a) ? static_cast<void>(0) : SOLID_THROW(#a " check failed"));

#define SOLID_CHECK2(a, msg) \
    (SOLID_LIKELY(a) ? static_cast<void>(0) : SOLID_THROW(#a " check failed: " << msg));

#define GET_3RD_ARG(arg1, arg2, arg3, ...) arg3

#define SOLID_CHECK_MACRO_CHOOSER(...)     \
    GET_3RD_ARG(__VA_ARGS__, SOLID_CHECK2, \
        SOLID_CHECK1, )

#define SOLID_CHECK(...) SOLID_CHECK_MACRO_CHOOSER(__VA_ARGS__) \
(__VA_ARGS__)

} //namespace solid
