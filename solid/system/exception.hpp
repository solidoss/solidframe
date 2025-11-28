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
#include "solid/system/error.hpp"
#include "solid/system/log.hpp"
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

#ifndef SOLID_FUNCTION_NAME
#ifdef SOLID_ON_WINDOWS
#define SOLID_FUNCTION_NAME __func__
#else
#define SOLID_FUNCTION_NAME __FUNCTION__
#endif
#endif

class RuntimeError : public std::runtime_error {
    const ErrorConditionT err_;

public:
    template <typename F>
    RuntimeError(const F& _rf, const char* const _file, const int _line, const char* const _fnc)
        : std::runtime_error(_rf(_file, _line, _fnc))
    {
    }

    template <typename F>
    RuntimeError(const ErrorConditionT& _err, const F& _rf, const char* const _file, const int _line, const char* const _fnc)
        : std::runtime_error(_rf(_file, _line, _fnc, _err))
        , err_(_err)
    {
    }

    const ErrorConditionT& error() const noexcept
    {
        return err_;
    }
};

#define solid_throw(x)                                                          \
    throw solid::RuntimeError(                                                  \
        [&](const char* const _file, const int _line, const char* const _fnc) { \
            std::ostringstream os;                                              \
            os << '[' << _file << '(' << _line << ")][" << _fnc << "] " << x;   \
            return os.str();                                                    \
        },                                                                      \
        __FILE__, __LINE__, static_cast<const char*>((SOLID_FUNCTION_NAME)))

#define solid_throw_log(l, x)                                                   \
    solid_log(l, Exception, x);                                                 \
    throw solid::RuntimeError(                                                  \
        [&](const char* const _file, const int _line, const char* const _fnc) { \
            std::ostringstream os;                                              \
            os << '[' << _file << '(' << _line << ")][" << _fnc << "] " << x;   \
            return os.str();                                                    \
        },                                                                      \
        __FILE__, __LINE__, static_cast<const char*>((SOLID_FUNCTION_NAME)))

#define solid_throw_error(c)                                                                                        \
    throw solid::RuntimeError((c),                                                                                  \
        [&](const char* const _file, const int _line, const char* const _fnc, const solid::ErrorConditionT& _err) { \
            std::ostringstream os;                                                                                  \
            os << '[' << _file << '(' << _line << ")][" << _fnc << "] " #c ":" << _err.message();                   \
            return os.str();                                                                                        \
        },                                                                                                          \
        __FILE__, __LINE__, SOLID_FUNCTION_NAME)

#define solid_check_error(a, c) \
    if (not(a)) [[unlikely]]    \
        solid_throw_error(c);

#define solid_check1(a)                       \
    if (!(a)) [[unlikely]] {                  \
        solid_throw("(" #a ") check failed"); \
    }

#define solid_check2(a, msg)                           \
    if (!(a)) [[unlikely]] {                           \
        solid_throw("(" #a ") check failed: " << msg); \
    }

#define solid_check_log2(a, l)                       \
    if (!(a)) [[unlikely]] {                         \
        solid_throw_log(l, "(" #a ") check failed"); \
    }

#define solid_check_log3(a, l, msg)                           \
    if (!(a)) [[unlikely]] {                                  \
        solid_throw_log(l, "(" #a ") check failed: " << msg); \
    }

// adapted from: https://stackoverflow.com/questions/9183993/msvc-variadic-macro-expansion/9338429#9338429
#define solid_check(...) SOLID_CALL_OVERLOAD(solid_check, __VA_ARGS__)
#define solid_check_log(...) SOLID_CALL_OVERLOAD(solid_check_log, __VA_ARGS__)
} // namespace solid
