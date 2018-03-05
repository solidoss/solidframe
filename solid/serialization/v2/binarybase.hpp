// solid/serialization/v2/binarybase.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/serialization/v2/error.hpp"
#include "solid/serialization/v2/typemapbase.hpp"
#include "solid/serialization/v2/typetraits.hpp"
#include "solid/utility/common.hpp"
#include <memory>
#include <utility>

namespace solid {
namespace serialization {
namespace v2 {
namespace binary {

struct Limits {
    Limits(
        size_t   _stringlimit    = InvalidSize(),
        size_t   _containerlimit = InvalidSize(),
        uint64_t _streamlimit    = InvalidSize())
        : stringlimit_(_stringlimit)
        , containerlimit_(_containerlimit)
        , streamlimit_(_streamlimit)
    {
    } //unlimited by default

    bool hasStream() const
    {
        return streamlimit_ != InvalidSize();
    }

    bool hasString() const
    {
        return stringlimit_ != InvalidSize();
    }

    bool hasContainer() const
    {
        return containerlimit_ != InvalidSize();
    }

    size_t string() const
    {
        return stringlimit_;
    }

    size_t container() const
    {
        return containerlimit_;
    }

    uint64_t stream() const
    {
        return streamlimit_;
    }

    void clear()
    {
        stringlimit_    = InvalidSize();
        containerlimit_ = InvalidIndex();
        streamlimit_    = InvalidIndex();
    }

    size_t   stringlimit_;
    size_t   containerlimit_;
    uint64_t streamlimit_;
};

class Base : public v2::Base {
public:
    const ErrorConditionT& error() const
    {
        return error_;
    }

    const Limits& limits() const
    {
        return limits_;
    }

protected:
    enum struct ReturnE {
        Done = 0,
        Continue,
        Wait
    };

    enum {
        InnerListRun,
        InnerListCache,

        //Add above
        InnerListCount,
    };

    Base() {}
    Base(const Limits& _rlimits)
        : limits_(_rlimits)
    {
    }

protected:
    Limits          limits_;
    ErrorConditionT error_;
};

#define SOLID_SERIALIZATION_BASIC(T)                                                  \
    template <class S>                                                                \
    inline void solidSerializeV2(S& _rs, const T& _rt, const char* _name)             \
    {                                                                                 \
        _rs.addBasic(_rt, _name);                                                     \
    }                                                                                 \
    template <class S>                                                                \
    inline void solidSerializeV2(S& _rs, T& _rt, const char* _name)                   \
    {                                                                                 \
        _rs.addBasic(_rt, _name);                                                     \
    }                                                                                 \
    template <class S, class Ctx>                                                     \
    inline void solidSerializeV2(S& _rs, T& _rt, Ctx& _rctx, const char* _name)       \
    {                                                                                 \
        _rs.addBasic(_rt, _name);                                                     \
    }                                                                                 \
    template <class S, class Ctx>                                                     \
    inline void solidSerializeV2(S& _rs, const T& _rt, Ctx& _rctx, const char* _name) \
    {                                                                                 \
        _rs.addBasic(_rt, _name);                                                     \
    }

SOLID_SERIALIZATION_BASIC(int8_t);
SOLID_SERIALIZATION_BASIC(uint8_t);
SOLID_SERIALIZATION_BASIC(int16_t);
SOLID_SERIALIZATION_BASIC(uint16_t);
SOLID_SERIALIZATION_BASIC(int32_t);
SOLID_SERIALIZATION_BASIC(uint32_t);
SOLID_SERIALIZATION_BASIC(int64_t);
SOLID_SERIALIZATION_BASIC(uint64_t);
SOLID_SERIALIZATION_BASIC(bool);
SOLID_SERIALIZATION_BASIC(std::string);

//pair

template <class S, class T1, class T2>
inline void solidSerializeV2(S& _rs, std::pair<T1, T2>& _rp, const char* _name)
{
    using FirstT = typename std::remove_reference<typename std::remove_const<T1>::type>::type;
    _rs.add(const_cast<FirstT&>(_rp.first), "first").add(_rp.second, "second");
}

template <class S, class T1, class T2>
inline void solidSerializeV2(S& _rs, const std::pair<T1, T2>& _rp, const char* _name)
{
    _rs.add(_rp.first, "first").add(_rp.second, "second");
}

template <class S, class T1, class T2, class Ctx>
inline void solidSerializeV2(S& _rs, std::pair<T1, T2>& _rp, Ctx& _rctx, const char* _name)
{
    using FirstT = typename std::remove_reference<typename std::remove_const<T1>::type>::type;
    _rs.add(const_cast<FirstT&>(_rp.first), _rctx, "first").add(_rp.second, _rctx, "second");
}

template <class S, class T1, class T2, class Ctx>
inline void solidSerializeV2(S& _rs, const std::pair<T1, T2>& _rp, Ctx& _rctx, const char* _name)
{
    _rs.add(_rp.first, _rctx, "first").add(_rp.second, _rctx, "second");
}

//dispatch

template <class S, class T>
inline void solidSerializeV2IsFunction(S& _rs, const T& _rt, const char* _name, std::true_type)
{
    _rs.addFunction(_rs, _rt, _name);
}

template <class S, class T, class Ctx>
inline void solidSerializeV2IsFunction(S& _rs, const T& _rt, Ctx& _rctx, const char* _name, std::true_type)
{
    _rs.addFunction(_rs, _rt, _rctx, _name);
}

template <class S, class T>
inline void solidSerializeV2IsContainer(S& _rs, const T& _rt, const char* _name, std::true_type)
{
    _rs.addContainer(_rs, _rt, _name);
}

template <class S, class T, class Ctx>
inline void solidSerializeV2IsContainer(S& _rs, const T& _rt, Ctx& _rctx, const char* _name, std::true_type)
{
    _rs.addContainer(_rs, _rt, _rctx, _name);
}

template <class S, class T, class Ctx>
inline void solidSerializeV2IsContainer(S& _rs, T& _rt, Ctx& _rctx, const char* _name, std::true_type)
{
    _rs.addContainer(_rs, _rt, _rctx, _name);
}

template <class S, class T>
inline void solidSerializeV2IsContainer(S& _rs, T& _rt, const char* _name, std::true_type)
{
    _rs.addContainer(_rs, _rt, _name);
}

template <class S, class T>
inline void solidSerializeV2IsContainer(S& _rs, T& _rt, const char* _name, std::false_type)
{
    _rt.solidSerializeV2(_rs, _name);
}

template <class S, class T, class Ctx>
inline void solidSerializeV2IsContainer(S& _rs, T& _rt, Ctx& _rctx, const char* _name, std::false_type)
{
    _rt.solidSerializeV2(_rs, _rctx, _name);
}

template <class S, class T, class Ctx>
inline void solidSerializeV2IsContainer(S& _rs, const T& _rt, Ctx& _rctx, const char* _name, std::false_type)
{
    _rt.solidSerializeV2(_rs, _rctx, _name);
}

template <class S, class T>
inline void solidSerializeV2IsFunction(S& _rs, T& _rt, const char* _name, std::false_type)
{
    solidSerializeV2IsContainer(_rs, _rt, _name, is_container<T>());
}

template <class S, class T, class Ctx>
inline void solidSerializeV2IsFunction(S& _rs, T& _rt, Ctx& _rctx, const char* _name, std::false_type)
{
    solidSerializeV2IsContainer(_rs, _rt, _rctx, _name, is_container<T>());
}

template <class S, class T, class Ctx>
inline void solidSerializeV2IsFunction(S& _rs, const T& _rt, Ctx& _rctx, const char* _name, std::false_type)
{
    solidSerializeV2IsContainer(_rs, _rt, _rctx, _name, is_container<T>());
}

template <class S, class T>
inline void solidSerializeV2(S& _rs, std::shared_ptr<T>& _rp, const char* _name)
{
    _rs.addPointer(_rp, _name);
}

template <class S, class T, class Ctx>
inline void solidSerializeV2(S& _rs, std::shared_ptr<T>& _rp, Ctx& _rctx, const char* _name)
{
    _rs.addPointer(_rp, _rctx, _name);
}

template <class S, class T>
inline void solidSerializeV2(S& _rs, const std::shared_ptr<T>& _rp, const char* _name)
{
    _rs.addPointer(_rp, _name);
}

template <class S, class T, class Ctx>
inline void solidSerializeV2(S& _rs, const std::shared_ptr<T>& _rp, Ctx& _rctx, const char* _name)
{
    _rs.addPointer(_rp, _rctx, _name);
}

template <class S, class T, class D>
inline void solidSerializeV2(S& _rs, std::unique_ptr<T, D>& _rp, const char* _name)
{
    _rs.addPointer(_rp, _name);
}

template <class S, class T, class D, class Ctx>
inline void solidSerializeV2(S& _rs, std::unique_ptr<T, D>& _rp, Ctx& _rctx, const char* _name)
{
    _rs.addPointer(_rp, _rctx, _name);
}

template <class S, class T, class D>
inline void solidSerializeV2(S& _rs, const std::unique_ptr<T, D>& _rp, const char* _name)
{
    _rs.addPointer(_rp, _name);
}

template <class S, class T, class D, class Ctx>
inline void solidSerializeV2(S& _rs, const std::unique_ptr<T, D>& _rp, Ctx& _rctx, const char* _name)
{
    _rs.addPointer(_rp, _rctx, _name);
}

template <class S, class T>
inline void solidSerializeV2(S& _rs, T& _rt, const char* _name)
{
    solidSerializeV2IsFunction(_rs, _rt, _name, is_callable<T, S&, const char*>());
}

template <class S, class T, class Ctx>
inline void solidSerializeV2(S& _rs, T& _rt, Ctx& _rctx, const char* _name)
{
    solidSerializeV2IsFunction(_rs, _rt, _rctx, _name, is_callable<T, S&, Ctx&, const char*>());
}

template <class S, class T>
inline void solidSerializeV2(S& _rs, const T& _rt, const char* _name)
{
    solidSerializeV2IsFunction(_rs, _rt, _name, is_callable<T, S&, const char*>());
}

template <class S, class T, class Ctx>
inline void solidSerializeV2(S& _rs, const T& _rt, Ctx& _rctx, const char* _name)
{
    solidSerializeV2IsFunction(_rs, _rt, _rctx, _name, is_callable<T, S&, Ctx&, const char*>());
}

// == SerializePush ===========================================================

//dispatch

template <class S, class T>
inline void solidSerializePushV2IsFunction(S& _rs, T&& _rt, const char* _name, std::true_type)
{
    _rs.pushFunction(_rs, std::move(_rt), _name);
}

template <class S, class T, class Ctx>
inline void solidSerializePushV2IsFunction(S& _rs, T&& _rt, Ctx& _rctx, const char* _name, std::true_type)
{
    _rs.pushFunction(_rs, std::move(_rt), _rctx, _name);
}

template <class S, class T>
inline void solidSerializePushV2(S& _rs, T&& _rt, const char* _name)
{
    solidSerializePushV2IsFunction(_rs, std::move(_rt), _name, is_callable<T, S&, const char*>());
}

template <class S, class T, class Ctx>
inline void solidSerializePushV2(S& _rs, T&& _rt, Ctx& _rctx, const char* _name)
{
    solidSerializePushV2IsFunction(_rs, std::move(_rt), _rctx, _name, is_callable<T, S&, Ctx&, const char*>());
}

} //namespace binary
} //namespace v2
} //namespace serialization
} //namespace solid

#define SOLID_SERIALIZE_CONTEXT_V2(ser, rthis, ctx, name)           \
    template <class S, class C>                                     \
    void solidSerializeV2(S& _s, C& _rctx, const char* _name) const \
    {                                                               \
        solidSerializeV2(_s, *this, _rctx, _name);                  \
    }                                                               \
    template <class S, class C>                                     \
    void solidSerializeV2(S& _s, C& _rctx, const char* _name)       \
    {                                                               \
        solidSerializeV2(_s, *this, _rctx, _name);                  \
    }                                                               \
    template <class S, class T, class C>                            \
    static void solidSerializeV2(S& ser, T& rthis, C& ctx, const char* name)
