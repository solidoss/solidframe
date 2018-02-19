#pragma once
#include "solid/serialization/v2/typetraits.hpp"
#include "solid/serialization/v2/error.hpp"
#include "solid/utility/common.hpp"
#include <memory>
namespace solid {
namespace serialization {
namespace v2 {
namespace binary {

struct Limits {
    Limits(
        size_t   _stringlimit    = InvalidSize(),
        size_t   _containerlimit = InvalidSize(),
        uint64_t _streamlimit    = InvalidSize())
        : stringlimit(_stringlimit)
        , containerlimit(_containerlimit)
        , streamlimit(_streamlimit)
    {
    } //unlimited by default

    size_t   stringlimit;
    size_t   containerlimit;
    uint64_t streamlimit;
};


class Base {
public:
    const ErrorConditionT& error()const{
        return error_;
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
protected:
    Limits limits_;
    ErrorConditionT error_;
};

#define SOLID_SERIALIZATION_BASIC(T)                                     \
    template <class S>                                                   \
    void solidSerializeV2(S& _rs, const T& _rt, const char* _name)       \
    {                                                                    \
        _rs.addBasic(_rt, _name);                                        \
    }                                                                    \
    template <class S, class Ctx>                                        \
    void solidSerializeV2(S& _rs, T& _rt, Ctx& _rctx, const char* _name) \
    {                                                                    \
        _rs.addBasic(_rt, _name);                                        \
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

//dispatch

template <class S, class T>
void solidSerializeV2IsFunction(S& _rs, const T& _rt, const char* _name, std::true_type)
{
    _rs.addFunction(_rs, _rt, _name);
}

template <class S, class T, class Ctx>
void solidSerializeV2IsFunction(S& _rs, const T& _rt, Ctx& _rctx, const char* _name, std::true_type)
{
    _rs.addFunction(_rs, _rt, _rctx, _name);
}

template <class S, class T>
void solidSerializeV2IsContainer(S& _rs, const T& _rt, const char* _name, std::true_type)
{
    _rs.addContainer(_rs, _rt, _name);
}

template <class S, class T, class Ctx>
void solidSerializeV2IsContainer(S& _rs, const T& _rt, Ctx& _rctx, const char* _name, std::true_type)
{
    _rs.addContainer(_rs, _rt, _rctx, _name);
}

template <class S, class T, class Ctx>
void solidSerializeV2IsContainer(S& _rs, T& _rt, Ctx& _rctx, const char* _name, std::true_type)
{
    _rs.addContainer(_rs, _rt, _rctx, _name);
}

template <class S, class T>
void solidSerializeV2IsContainer(S& _rs, T& _rt, const char* _name, std::false_type)
{
    _rt.solidSerializeV2(_rs, _name);
}

template <class S, class T, class Ctx>
void solidSerializeV2IsContainer(S& _rs, T& _rt, Ctx& _rctx, const char* _name, std::false_type)
{
    _rt.solidSerializeV2(_rs, _rctx, _name);
}

template <class S, class T>
void solidSerializeV2IsFunction(S& _rs, T& _rt, const char* _name, std::false_type)
{
    solidSerializeV2IsContainer(_rs, _rt, _name, is_container<T>());
}

template <class S, class T, class Ctx>
void solidSerializeV2IsFunction(S& _rs, T& _rt, Ctx& _rctx, const char* _name, std::false_type)
{
    solidSerializeV2IsContainer(_rs, _rt, _rctx, _name, is_container<T>());
}

template <class S, class T>
void solidSerializeV2(S& _rs, std::shared_ptr<T>& _rp, const char* _name)
{
    _rs.addPointer(_rs, _rp, _name);
}

template <class S, class T, class Ctx>
void solidSerializeV2(S& _rs, std::shared_ptr<T>& _rp, Ctx& _rctx, const char* _name)
{
    _rs.addPointer(_rs, _rp, _rctx, _name);
}

template <class S, class T>
void solidSerializeV2(S& _rs, const std::shared_ptr<T>& _rp, const char* _name)
{
    _rs.addPointer(_rs, _rp, _name);
}

template <class S, class T, class Ctx>
void solidSerializeV2(S& _rs, const std::shared_ptr<T>& _rp, Ctx& _rctx, const char* _name)
{
    _rs.addPointer(_rs, _rp, _rctx, _name);
}

template <class S, class T, class D>
void solidSerializeV2(S& _rs, std::unique_ptr<T, D>& _rp, const char* _name)
{
    _rs.addPointer(_rs, _rp, _name);
}

template <class S, class T, class D, class Ctx>
void solidSerializeV2(S& _rs, std::unique_ptr<T, D>& _rp, Ctx& _rctx, const char* _name)
{
    _rs.addPointer(_rs, _rp, _rctx, _name);
}

template <class S, class T, class D>
void solidSerializeV2(S& _rs, const std::unique_ptr<T, D>& _rp, const char* _name)
{
    _rs.addPointer(_rs, _rp, _name);
}

template <class S, class T, class D, class Ctx>
void solidSerializeV2(S& _rs, const std::unique_ptr<T, D>& _rp, Ctx& _rctx, const char* _name)
{
    _rs.addPointer(_rs, _rp, _rctx, _name);
}

template <class S, class T>
void solidSerializeV2(S& _rs, T& _rt, const char* _name)
{
    solidSerializeV2IsFunction(_rs, _rt, _name, is_callable<T, S&, const char*>());
}

template <class S, class T, class Ctx>
void solidSerializeV2(S& _rs, T& _rt, Ctx& _rctx, const char* _name)
{
    solidSerializeV2IsFunction(_rs, _rt, _rctx, _name, is_callable<T, S&, Ctx&, const char*>());
}

template <class S, class T>
void solidSerializeV2(S& _rs, const T& _rt, const char* _name)
{
    solidSerializeV2IsFunction(_rs, _rt, _name, is_callable<T, S&, const char*>());
}

template <class S, class T, class Ctx>
void solidSerializeV2(S& _rs, const T& _rt, Ctx& _rctx, const char* _name)
{
    solidSerializeV2IsFunction(_rs, _rt, _rctx, _name, is_callable<T, S&, Ctx&, const char*>());
}

// == SerializePush ===========================================================

//dispatch

template <class S, class T>
void solidSerializePushV2IsFunction(S& _rs, T&& _rt, const char* _name, std::true_type)
{
    _rs.pushFunction(_rs, std::move(_rt), _name);
}

template <class S, class T, class Ctx>
void solidSerializePushV2IsFunction(S& _rs, T&& _rt, Ctx& _rctx, const char* _name, std::true_type)
{
    _rs.pushFunction(_rs, std::move(_rt), _rctx, _name);
}

template <class S, class T>
void solidSerializePushV2(S& _rs, T&& _rt, const char* _name)
{
    solidSerializePushV2IsFunction(_rs, std::move(_rt), _name, is_callable<T, S&, const char*>());
}

template <class S, class T, class Ctx>
void solidSerializePushV2(S& _rs, T&& _rt, Ctx& _rctx, const char* _name)
{
    solidSerializePushV2IsFunction(_rs, std::move(_rt), _rctx, _name, is_callable<T, S&, Ctx&, const char*>());
}

} //namespace binary
} //namespace v2
} //namespace serialization
} //namespace solid
