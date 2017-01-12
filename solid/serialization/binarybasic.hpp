// solid/serialization/binarybasic.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_SERIALIZATION_BINARY_BASIC_HPP
#define SOLID_SERIALIZATION_BINARY_BASIC_HPP

#include "solid/utility/algorithm.hpp"
#include <cstring>
#include <array>

namespace solid{
namespace serialization{
namespace binary{

#define BASIC_DECL(tp) \
template <class S>\
void solidSerialize(S &_s, tp &_t){\
    _s.push(_t, "basic");\
}\
template <class S, class Ctx>\
void solidSerialize(S &_s, tp &_t, Ctx &){\
    _s.push(_t, "basic");\
}

inline char *store(char *_pd, const uint8_t _val){
    uint8_t *pd = reinterpret_cast<uint8_t*>(_pd);
    *pd = _val;
    return _pd + 1;
}

inline char *store(char *_pd, const uint16_t _val){
    uint8_t *pd = reinterpret_cast<uint8_t*>(_pd);
    *(pd)       = ((_val >> 8) & 0xff);
    *(pd + 1)   = (_val & 0xff);
    return _pd + 2;
}

inline char *store(char *_pd, const uint32_t _val){

    _pd = store(_pd, static_cast<uint16_t>(_val >> 16));

    return store(_pd, static_cast<uint16_t>(_val & 0xffff));;
}

inline char *store(char *_pd, const uint64_t _val){

    _pd = store(_pd, static_cast<uint32_t>(_val >> 32));

    return store(_pd, static_cast<uint32_t>(_val & 0xffffffffULL));;
}

template <size_t S>
inline char *store(char *_pd, const std::array<uint8_t, S> _val){
    memcpy(_pd, _val.data(), S);
    return _pd + S;
}


inline const char* load(const char *_ps, uint8_t &_val){
    const uint8_t *ps = reinterpret_cast<const uint8_t*>(_ps);
    _val = *ps;
    return _ps + 1;
}

inline const char* load(const char *_ps, uint16_t &_val){
    const uint8_t *ps = reinterpret_cast<const uint8_t*>(_ps);
    _val = *ps;
    _val <<= 8;
    _val |= *(ps + 1);
    return _ps + 2;
}

inline const char* load(const char *_ps, uint32_t &_val){
    uint16_t    upper;
    uint16_t    lower;
    _ps = load(_ps, upper);
    _ps = load(_ps, lower);
    _val = upper;
    _val <<= 16;
    _val |= lower;
    return _ps;
}

inline const char* load(const char *_ps, uint64_t &_val){
    uint32_t    upper;
    uint32_t    lower;
    _ps = load(_ps, upper);
    _ps = load(_ps, lower);
    _val = upper;
    _val <<= 32;
    _val |= lower;
    return _ps;
}
template <size_t S>
inline const char *load(const char *_ps, std::array<uint8_t, S> &_val){
    memcpy(_val.data, _ps, S);
    return _ps + S;
}

//cross integer serialization

inline size_t crossSize(const char *_ps){
    const uint8_t *ps = reinterpret_cast<const uint8_t*>(_ps);
    uint8_t         v = *ps;
    const bool  ok = check_value_with_crc(v, v);
    if(ok){
        return v + 1;
    }
    return -1;
}

inline size_t crossSize(uint8_t _v){
    return max_padded_byte_cout(_v) + 1;
}

inline size_t crossSize(uint16_t _v){
    return max_padded_byte_cout(_v) + 1;
}

inline size_t crossSize(uint32_t _v){
    return max_padded_byte_cout(_v) + 1;
}
inline size_t crossSize(uint64_t _v){
    return max_padded_byte_cout(_v) + 1;
}

char* crossStore(char *_pd, uint8_t _v);
char* crossStore(char *_pd, uint16_t _v);
char* crossStore(char *_pd, uint32_t _v);
char* crossStore(char *_pd, uint64_t _v);

const char* crossLoad(const char *_ps, uint8_t &_val);
const char* crossLoad(const char *_ps, uint16_t &_val);
const char* crossLoad(const char *_ps, uint32_t &_val);
const char* crossLoad(const char *_ps, uint64_t &_val);


}//namespace binary
}//namespace serialization
}//namespace solid

#endif
