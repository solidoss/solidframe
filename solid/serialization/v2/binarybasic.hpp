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

#include "solid/system/convertors.hpp"
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
#ifdef SOLID_ON_BIG_ENDIAN
    *(pd)     = ((_val >> 8) & 0xff);
    *(pd + 1) = (_val & 0xff);
#else
    *(pd)     = (_val & 0xff);
    *(pd + 1) = ((_val >> 8) & 0xff);
#endif
    return _pd + 2;
}

inline char* store(char* _pd, const uint32_t _val)
{
    uint8_t* pd = reinterpret_cast<uint8_t*>(_pd);
#ifdef SOLID_ON_BIG_ENDIAN
    *(pd)     = ((_val >> 24) & 0xff);
    *(pd + 1) = ((_val >> 16) & 0xff);
    *(pd + 2) = ((_val >> 8) & 0xff);
    *(pd + 3) = (_val & 0xff);
#else
    *(pd)     = (_val & 0xff);
    *(pd + 1) = ((_val >> 8) & 0xff);
    *(pd + 2) = ((_val >> 16) & 0xff);
    *(pd + 3) = ((_val >> 24) & 0xff);
#endif
    return _pd + 4;
}

inline char* store(char* _pd, const uint64_t _val)
{
    uint8_t* pd = reinterpret_cast<uint8_t*>(_pd);
#ifdef SOLID_ON_BIG_ENDIAN
    *(pd + 0) = ((_val >> 56) & 0xff);
    *(pd + 1) = ((_val >> 48) & 0xff);
    *(pd + 2) = ((_val >> 40) & 0xff);
    *(pd + 3) = ((_val >> 32) & 0xff);
    *(pd + 4) = ((_val >> 24) & 0xff);
    *(pd + 5) = ((_val >> 16) & 0xff);
    *(pd + 6) = ((_val >> 8) & 0xff);
    *(pd + 7) = (_val & 0xff);
#else
    *(pd + 0) = (_val & 0xff);
    *(pd + 1) = ((_val >> 8) & 0xff);
    *(pd + 2) = ((_val >> 16) & 0xff);
    *(pd + 3) = ((_val >> 24) & 0xff);
    *(pd + 4) = ((_val >> 32) & 0xff);
    *(pd + 5) = ((_val >> 40) & 0xff);
    *(pd + 6) = ((_val >> 48) & 0xff);
    *(pd + 7) = ((_val >> 56) & 0xff);
#endif
    return _pd + 8;
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
    uint16_t       v  = *ps;
#ifdef SOLID_ON_BIG_ENDIAN
    v = (v << 8) | *(ps + 1);
#else
    v |= (static_cast<uint16_t>(*(ps + 1) << 8));
#endif
    _val = v;
    return _ps + 2;
}

inline const char* load(const char* _ps, uint32_t& _val)
{
    const uint8_t* ps = reinterpret_cast<const uint8_t*>(_ps);
    uint32_t       v  = *ps;
#ifdef SOLID_ON_BIG_ENDIAN
    v = (v << 8) | *(ps + 1);
    v = (v << 8) | *(ps + 2);
    v = (v << 8) | *(ps + 3);
#else
    v |= (static_cast<uint32_t>(*(ps + 1)) << 8);
    v |= (static_cast<uint32_t>(*(ps + 2)) << 16);
    v |= (static_cast<uint32_t>(*(ps + 3)) << 24);
#endif

    _val = v;
    return _ps + 4;
}

inline const char* load(const char* _ps, uint64_t& _val)
{
    const uint8_t* ps = reinterpret_cast<const uint8_t*>(_ps);
    uint64_t       v  = *ps;

#ifdef SOLID_ON_BIG_ENDIAN
    v = (v << 8) | *(ps + 1);
    v = (v << 8) | *(ps + 2);
    v = (v << 8) | *(ps + 3);
    v = (v << 8) | *(ps + 4);
    v = (v << 8) | *(ps + 5);
    v = (v << 8) | *(ps + 6);
    v = (v << 8) | *(ps + 7);
#else
    v |= (static_cast<uint64_t>(*(ps + 1)) << 8);
    v |= (static_cast<uint64_t>(*(ps + 2)) << 16);
    v |= (static_cast<uint64_t>(*(ps + 3)) << 24);
    v |= (static_cast<uint64_t>(*(ps + 4)) << 32);
    v |= (static_cast<uint64_t>(*(ps + 5)) << 40);
    v |= (static_cast<uint64_t>(*(ps + 6)) << 48);
    v |= (static_cast<uint64_t>(*(ps + 7)) << 56);
#endif

    _val = v;
    return _ps + 8;
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

char* store_with_check(char* _pd, const size_t _sz, uint8_t _v);
char* store_with_check(char* _pd, const size_t _sz, uint16_t _v);

inline char* store_with_check(char* _pd, const size_t _sz, uint32_t _v)
{
#ifdef SOLID_ON_BIG_ENDIAN
    _v = swap_bytes(_v);
#endif
    uint8_t*     pd = reinterpret_cast<uint8_t*>(_pd);
    const size_t sz = max_padded_byte_cout(_v);
    if ((sz + 1) <= _sz) {
        const bool ok = compute_value_with_crc(*pd, static_cast<uint8_t>(sz));
        if (ok) {
            ++pd;

            switch (sz) {
            case 0:
                break;
            case 1:
                *pd = (_v & 0xff);
                break;
            case 2:
                *(pd)     = (_v & 0xff);
                *(pd + 1) = ((_v >> 8) & 0xff);
                break;
            case 3:
                *(pd)     = (_v & 0xff);
                *(pd + 1) = ((_v >> 8) & 0xff);
                *(pd + 2) = ((_v >> 16) & 0xff);
                break;
            case 4:
                *(pd)     = (_v & 0xff);
                *(pd + 1) = ((_v >> 8) & 0xff);
                *(pd + 2) = ((_v >> 16) & 0xff);
                *(pd + 3) = ((_v >> 24) & 0xff);
                break;
            default:
                return nullptr;
            }

            return _pd + sz + 1;
        }
    }
    return nullptr;
}

inline char* store_with_check(char* _pd, const size_t _sz, uint64_t _v)
{
#ifdef SOLID_ON_BIG_ENDIAN
    _v = swap_bytes(_v);
#endif
    uint8_t*     pd = reinterpret_cast<uint8_t*>(_pd);
    const size_t sz = max_padded_byte_cout(_v);
    if ((sz + 1) <= _sz) {
        const bool ok = compute_value_with_crc(*pd, static_cast<uint8_t>(sz));
        if (ok) {
            ++pd;

            switch (sz) {
            case 0:
                break;
            case 1:
                *pd = (_v & 0xff);
                break;
            case 2:
                *(pd)     = (_v & 0xff);
                *(pd + 1) = ((_v >> 8) & 0xff);
                break;
            case 3:
                *(pd)     = (_v & 0xff);
                *(pd + 1) = ((_v >> 8) & 0xff);
                *(pd + 2) = ((_v >> 16) & 0xff);
                break;
            case 4:
                *(pd)     = (_v & 0xff);
                *(pd + 1) = ((_v >> 8) & 0xff);
                *(pd + 2) = ((_v >> 16) & 0xff);
                *(pd + 3) = ((_v >> 24) & 0xff);
                break;
            case 5:
                *(pd)     = (_v & 0xff);
                *(pd + 1) = ((_v >> 8) & 0xff);
                *(pd + 2) = ((_v >> 16) & 0xff);
                *(pd + 3) = ((_v >> 24) & 0xff);
                *(pd + 4) = ((_v >> 32) & 0xff);
                break;
            case 6:
                *(pd)     = (_v & 0xff);
                *(pd + 1) = ((_v >> 8) & 0xff);
                *(pd + 2) = ((_v >> 16) & 0xff);
                *(pd + 3) = ((_v >> 24) & 0xff);
                *(pd + 4) = ((_v >> 32) & 0xff);
                *(pd + 5) = ((_v >> 40) & 0xff);
                break;
            case 7:
                *(pd)     = (_v & 0xff);
                *(pd + 1) = ((_v >> 8) & 0xff);
                *(pd + 2) = ((_v >> 16) & 0xff);
                *(pd + 3) = ((_v >> 24) & 0xff);
                *(pd + 4) = ((_v >> 32) & 0xff);
                *(pd + 5) = ((_v >> 40) & 0xff);
                *(pd + 6) = ((_v >> 48) & 0xff);
                break;
            case 8:
                *(pd)     = (_v & 0xff);
                *(pd + 1) = ((_v >> 8) & 0xff);
                *(pd + 2) = ((_v >> 16) & 0xff);
                *(pd + 3) = ((_v >> 24) & 0xff);
                *(pd + 4) = ((_v >> 32) & 0xff);
                *(pd + 5) = ((_v >> 40) & 0xff);
                *(pd + 6) = ((_v >> 48) & 0xff);
                *(pd + 7) = ((_v >> 56) & 0xff);
                break;
            default:
                return nullptr;
            }

            return _pd + sz + 1;
        }
    }
    return nullptr;
}

const char* load_with_check(const char* _ps, const size_t _sz, uint8_t& _val);
const char* load_with_check(const char* _ps, const size_t _sz, uint16_t& _val);

inline const char* load_with_check(const char* _ps, const size_t _sz, uint32_t& _val)
{
    if (_sz != 0) {
        const uint8_t* ps  = reinterpret_cast<const uint8_t*>(_ps);
        uint8_t        vsz = *ps;
        const bool     ok  = check_value_with_crc(vsz, vsz);
        const size_t   sz  = vsz;
        uint32_t       v   = 0;

        if (ok && (sz + 1) <= _sz) {
            ++ps;

            switch (sz) {
            case 0:
                v = 0;
                break;
            case 1:
                v = *ps;
                break;
            case 2:
                v = *ps;
                v |= static_cast<uint32_t>(*(ps + 1)) << 8;
                break;
            case 3:
                v = *ps;
                v |= static_cast<uint32_t>(*(ps + 1)) << 8;
                v |= static_cast<uint32_t>(*(ps + 2)) << 16;
                break;
            case 4:
                v = *ps;
                v |= static_cast<uint32_t>(*(ps + 1)) << 8;
                v |= static_cast<uint32_t>(*(ps + 2)) << 16;
                v |= static_cast<uint32_t>(*(ps + 3)) << 24;
                break;
            default:
                return nullptr;
            }
#ifdef SOLID_ON_BIG_ENDIAN
            _val = swap_bytes(v);
#else
            _val = v;
#endif
            return _ps + static_cast<size_t>(vsz) + 1;
        }
    }
    return nullptr;
}

inline const char* load_without_check(const char* _ps, const size_t _sz, uint64_t& _val){
    const uint8_t* ps  = reinterpret_cast<const uint8_t*>(_ps);
    const size_t   sz  = _sz;
    uint64_t       v   = 0;

    switch (sz) {
    case 0:
        v = 0;
        break;
    case 1:
        v = *ps;
        break;
    case 2:
        v = *ps;
        v |= static_cast<uint64_t>(*(ps + 1)) << 8;
        break;
    case 3:
        v = *ps;
        v |= static_cast<uint64_t>(*(ps + 1)) << 8;
        v |= static_cast<uint64_t>(*(ps + 2)) << 16;
        break;
    case 4:
        v = *ps;
        v |= static_cast<uint64_t>(*(ps + 1)) << 8;
        v |= static_cast<uint64_t>(*(ps + 2)) << 16;
        v |= static_cast<uint64_t>(*(ps + 3)) << 24;
        break;
    case 5:
        v = *ps;
        v |= static_cast<uint64_t>(*(ps + 1)) << 8;
        v |= static_cast<uint64_t>(*(ps + 2)) << 16;
        v |= static_cast<uint64_t>(*(ps + 3)) << 24;
        v |= static_cast<uint64_t>(*(ps + 4)) << 32;
        break;
    case 6:
        v = *ps;
        v |= static_cast<uint64_t>(*(ps + 1)) << 8;
        v |= static_cast<uint64_t>(*(ps + 2)) << 16;
        v |= static_cast<uint64_t>(*(ps + 3)) << 24;
        v |= static_cast<uint64_t>(*(ps + 4)) << 32;
        v |= static_cast<uint64_t>(*(ps + 5)) << 40;
        break;
    case 7:
        v = *ps;
        v |= static_cast<uint64_t>(*(ps + 1)) << 8;
        v |= static_cast<uint64_t>(*(ps + 2)) << 16;
        v |= static_cast<uint64_t>(*(ps + 3)) << 24;
        v |= static_cast<uint64_t>(*(ps + 4)) << 32;
        v |= static_cast<uint64_t>(*(ps + 5)) << 40;
        v |= static_cast<uint64_t>(*(ps + 6)) << 48;
        break;
    case 8:
        v = *ps;
        v |= static_cast<uint64_t>(*(ps + 1)) << 8;
        v |= static_cast<uint64_t>(*(ps + 2)) << 16;
        v |= static_cast<uint64_t>(*(ps + 3)) << 24;
        v |= static_cast<uint64_t>(*(ps + 4)) << 32;
        v |= static_cast<uint64_t>(*(ps + 5)) << 40;
        v |= static_cast<uint64_t>(*(ps + 6)) << 48;
        v |= static_cast<uint64_t>(*(ps + 7)) << 56;
        break;
    default:
        return nullptr;
    }
#ifdef SOLID_ON_BIG_ENDIAN
    _val = swap_bytes(v);
#else
    _val = v;
#endif
    return _ps + sz;
}

inline const char* load_with_check(const char* _ps, const size_t _sz, uint64_t& _val)
{
    if (_sz != 0) {
        const uint8_t* ps  = reinterpret_cast<const uint8_t*>(_ps);
        uint8_t        vsz = *ps;
        const bool     ok  = check_value_with_crc(vsz, vsz);
        const size_t   sz  = vsz;
        
        if (ok && (sz + 1) <= _sz) {
            return load_without_check(_ps + 1, sz, _val);
        }
    }
    return nullptr;
}
} //namespace cross

inline void store_bit_at(uint8_t* _pbeg, const size_t _bit_idx, const bool _opt)
{
    _pbeg += (_bit_idx >> 3);
    const size_t  bit_off = _bit_idx & 7;
    const uint8_t opt     = _opt;
    //clear the bit
    *_pbeg &= ~static_cast<uint8_t>(1 << bit_off);
    *_pbeg |= ((opt & 1) << bit_off);
}

inline bool load_bit_from(const uint8_t* _pbeg, const size_t _bit_idx)
{
    static const bool b[2] = {false, true};
    _pbeg += (_bit_idx >> 3);
    const size_t bit_off = _bit_idx & 7;
    return b[(*_pbeg >> bit_off) & 1];
}

} //namespace binary
} //namespace v2
} //namespace serialization
} //namespace solid
