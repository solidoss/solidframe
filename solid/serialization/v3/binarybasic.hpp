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

#include "solid/system/cassert.hpp"
#include "solid/system/convertors.hpp"
#include "solid/utility/algorithm.hpp"
#include <array>
#include <cstring>

namespace solid {
namespace serialization {
namespace v3 {
namespace binary {

namespace impl {
inline char* store(char* _pd, const uint8_t _val, TypeToType<uint8_t> _ff)
{
    uint8_t* pd = reinterpret_cast<uint8_t*>(_pd);
    *pd         = _val;
    return _pd + 1;
}

union Convert16 {
    uint16_t value_;
    uint8_t  bytes_[sizeof(uint16_t)];
};

inline char* store(char* _pd, const uint16_t _val, TypeToType<uint16_t> _ff)
{
    uint8_t*  pd = reinterpret_cast<uint8_t*>(_pd);
    Convert16 c;
    c.value_ = _val;
#ifdef SOLID_ON_BIG_ENDIAN
    *(pd + 0) = c.bytes_[1];
    *(pd + 1) = c.bytes_[0];
#else
    *(pd + 0) = c.bytes_[0];
    *(pd + 1) = c.bytes_[1];
#endif
    return _pd + 2;
}

union Convert32 {
    uint32_t value_;
    uint8_t  bytes_[sizeof(uint32_t)];
};

inline char* store(char* _pd, const uint32_t _val, TypeToType<uint32_t> _ff)
{
    uint8_t*  pd = reinterpret_cast<uint8_t*>(_pd);
    Convert32 c;
    c.value_ = _val;
#ifdef SOLID_ON_BIG_ENDIAN
    *(pd + 0) = c.bytes_[3];
    *(pd + 1) = c.bytes_[2];
    *(pd + 2) = c.bytes_[1];
    *(pd + 3) = c.bytes_[0];
#else
    *(pd + 0) = c.bytes_[0];
    *(pd + 1) = c.bytes_[1];
    *(pd + 2) = c.bytes_[2];
    *(pd + 3) = c.bytes_[3];
#endif
    return _pd + 4;
}

union Convert64 {
    uint64_t value_;
    uint8_t  bytes_[sizeof(uint64_t)];
};

inline char* store(char* _pd, const uint64_t _val, TypeToType<uint64_t> _ff)
{
    uint8_t*  pd = reinterpret_cast<uint8_t*>(_pd);
    Convert64 c;
    c.value_ = _val;
#ifdef SOLID_ON_BIG_ENDIAN
    *(pd + 0) = c.bytes_[7];
    *(pd + 1) = c.bytes_[6];
    *(pd + 2) = c.bytes_[5];
    *(pd + 3) = c.bytes_[4];
    *(pd + 4) = c.bytes_[3];
    *(pd + 5) = c.bytes_[2];
    *(pd + 6) = c.bytes_[1];
    *(pd + 7) = c.bytes_[0];
#else
    *(pd + 0) = c.bytes_[0];
    *(pd + 1) = c.bytes_[1];
    *(pd + 2) = c.bytes_[2];
    *(pd + 3) = c.bytes_[3];
    *(pd + 4) = c.bytes_[4];
    *(pd + 5) = c.bytes_[5];
    *(pd + 6) = c.bytes_[6];
    *(pd + 7) = c.bytes_[7];
#endif
    return _pd + 8;
}

template <typename T>
union BytesConvertor {
    T    value_;
    char bytes_[sizeof(T)];

    BytesConvertor(const T& _value = 0)
        : value_(_value)
    {
    }
};

} // namespace impl

template <typename T>
inline char* store(char* _pd, const T _v)
{
    return impl::store(_pd, _v, TypeToType<T>());
}

inline const char* load(const char* _ps, uint8_t& _val)
{
    const uint8_t* ps = reinterpret_cast<const uint8_t*>(_ps);
    _val              = *ps;
    return _ps + 1;
}

inline const char* load(const char* _ps, uint16_t& _val)
{
    const uint8_t*  ps = reinterpret_cast<const uint8_t*>(_ps);
    impl::Convert16 c;
#ifdef SOLID_ON_BIG_ENDIAN
    c.bytes_[0] = *(ps + 1);
    c.bytes_[1] = *(ps + 0);
#else
    c.bytes_[0] = *(ps + 0);
    c.bytes_[1] = *(ps + 1);
#endif
    _val = c.value_;
    return _ps + 2;
}

inline const char* load(const char* _ps, uint32_t& _val)
{
    const uint8_t*  ps = reinterpret_cast<const uint8_t*>(_ps);
    impl::Convert32 c;
#ifdef SOLID_ON_BIG_ENDIAN
    c.bytes_[0] = *(ps + 3);
    c.bytes_[1] = *(ps + 2);
    c.bytes_[2] = *(ps + 1);
    c.bytes_[3] = *(ps + 0);
#else
    c.bytes_[0] = *(ps + 0);
    c.bytes_[1] = *(ps + 1);
    c.bytes_[2] = *(ps + 2);
    c.bytes_[3] = *(ps + 3);
#endif
    _val = c.value_;
    return _ps + 4;
}

template <size_t S>
inline char* store(char* _pd, const std::array<uint8_t, S> _val)
{
    memcpy(_pd, _val.data(), S);
    return _pd + S;
}

inline const char* load(const char* _ps, uint64_t& _val)
{
    const uint8_t*  ps = reinterpret_cast<const uint8_t*>(_ps);
    impl::Convert64 c;
#ifdef SOLID_ON_BIG_ENDIAN
    c.bytes_[0] = *(ps + 7);
    c.bytes_[1] = *(ps + 6);
    c.bytes_[2] = *(ps + 5);
    c.bytes_[3] = *(ps + 4);
    c.bytes_[4] = *(ps + 3);
    c.bytes_[5] = *(ps + 2);
    c.bytes_[6] = *(ps + 1);
    c.bytes_[7] = *(ps + 0);
#else
    c.bytes_[0] = *(ps + 0);
    c.bytes_[1] = *(ps + 1);
    c.bytes_[2] = *(ps + 2);
    c.bytes_[3] = *(ps + 3);
    c.bytes_[4] = *(ps + 4);
    c.bytes_[5] = *(ps + 5);
    c.bytes_[6] = *(ps + 6);
    c.bytes_[7] = *(ps + 7);
#endif
    _val = c.value_;
    return _ps + 8;
}

template <size_t S>
inline const char* load(const char* _ps, std::array<uint8_t, S>& _val)
{
    memcpy(_val.data, _ps, S);
    return _ps + S;
}

inline char* store_compact(char* _pd, const size_t _sz, uint8_t _v)
{
    int8_t* pd = reinterpret_cast<int8_t*>(_pd);
    if (_v <= static_cast<uint8_t>(std::numeric_limits<int8_t>::max())) {
        if (_sz >= 1) {
            *pd = static_cast<int8_t>(_v);
            return _pd + 1;
        }
    } else {
        if (_sz >= 2) {
            *pd       = -1;
            *(pd + 1) = static_cast<int8_t>(_v);
            return _pd + 2;
        }
    }
    return nullptr;
}

inline char* store_compact(char* _pd, const size_t _sz, uint16_t _v)
{
    static_assert(EndianessE::Native == EndianessE::Little, "Only little endian is supported");
    int8_t* pd = reinterpret_cast<int8_t*>(_pd);
    if (_v <= static_cast<uint16_t>(std::numeric_limits<int8_t>::max())) {
        if (_sz >= 1) {
            *pd = static_cast<int8_t>(_v);
            return _pd + 1;
        }
    } else {
        const size_t sz = max_padded_byte_cout(_v);
        if ((sz + 1) <= _sz) {
            *pd = -(static_cast<int8_t>(sz));

            const impl::BytesConvertor<uint16_t> to_bytes{_v};

            switch (sz) {
            case 1:
                *(_pd + 1) = to_bytes.bytes_[0];
                break;
            case 2:
                *(_pd + 1) = to_bytes.bytes_[0];
                *(_pd + 2) = to_bytes.bytes_[1];
                break;
            default:
                solid_assert(false);
                return nullptr;
            }
            return _pd + sz + 1;
        }
    }
    return nullptr;
}

inline char* store_compact(char* _pd, const size_t _sz, uint32_t _v)
{
    static_assert(EndianessE::Native == EndianessE::Little, "Only little endian is supported");
    int8_t* pd = reinterpret_cast<int8_t*>(_pd);
    if (_v <= static_cast<uint32_t>(std::numeric_limits<int8_t>::max())) {
        if (_sz >= 1) {
            *pd = static_cast<int8_t>(_v);
            return _pd + 1;
        }
    } else {
        const size_t sz = max_padded_byte_cout(_v);
        if ((sz + 1) <= _sz) {
            *pd = -(static_cast<int8_t>(sz));

            const impl::BytesConvertor<uint32_t> to_bytes{_v};

            switch (sz) {
            case 1:
                *(_pd + 1) = to_bytes.bytes_[0];
                break;
            case 2:
                *(_pd + 1) = to_bytes.bytes_[0];
                *(_pd + 2) = to_bytes.bytes_[1];
                break;
            case 3:
                *(_pd + 1) = to_bytes.bytes_[0];
                *(_pd + 2) = to_bytes.bytes_[1];
                *(_pd + 3) = to_bytes.bytes_[2];
                break;
            case 4:
                *(_pd + 1) = to_bytes.bytes_[0];
                *(_pd + 2) = to_bytes.bytes_[1];
                *(_pd + 3) = to_bytes.bytes_[2];
                *(_pd + 4) = to_bytes.bytes_[3];
                break;
            default:
                solid_assert(false);
                return nullptr;
            }
            return _pd + sz + 1;
        }
    }
    return nullptr;
}

inline char* store_compact(char* _pd, const size_t _sz, uint64_t _v)
{
    static_assert(EndianessE::Native == EndianessE::Little, "Only little endian is supported");
    int8_t* pd = reinterpret_cast<int8_t*>(_pd);
    if (_v <= static_cast<uint64_t>(std::numeric_limits<int8_t>::max())) {
        if (_sz >= 1) {
            *pd = static_cast<int8_t>(_v);
            return _pd + 1;
        }
    } else {
        const size_t sz = max_padded_byte_cout(_v);
        if ((sz + 1) <= _sz) {
            *pd = -(static_cast<int8_t>(sz));

            const impl::BytesConvertor<uint64_t> to_bytes{_v};

            switch (sz) {
            case 1:
                *(_pd + 1) = to_bytes.bytes_[0];
                break;
            case 2:
                *(_pd + 1) = to_bytes.bytes_[0];
                *(_pd + 2) = to_bytes.bytes_[1];
                break;
            case 3:
                *(_pd + 1) = to_bytes.bytes_[0];
                *(_pd + 2) = to_bytes.bytes_[1];
                *(_pd + 3) = to_bytes.bytes_[2];
                break;
            case 4:
                *(_pd + 1) = to_bytes.bytes_[0];
                *(_pd + 2) = to_bytes.bytes_[1];
                *(_pd + 3) = to_bytes.bytes_[2];
                *(_pd + 4) = to_bytes.bytes_[3];
                break;
            case 5:
                *(_pd + 1) = to_bytes.bytes_[0];
                *(_pd + 2) = to_bytes.bytes_[1];
                *(_pd + 3) = to_bytes.bytes_[2];
                *(_pd + 4) = to_bytes.bytes_[3];
                *(_pd + 5) = to_bytes.bytes_[4];
                break;
            case 6:
                *(_pd + 1) = to_bytes.bytes_[0];
                *(_pd + 2) = to_bytes.bytes_[1];
                *(_pd + 3) = to_bytes.bytes_[2];
                *(_pd + 4) = to_bytes.bytes_[3];
                *(_pd + 5) = to_bytes.bytes_[4];
                *(_pd + 6) = to_bytes.bytes_[5];
                break;
            case 7:
                *(_pd + 1) = to_bytes.bytes_[0];
                *(_pd + 2) = to_bytes.bytes_[1];
                *(_pd + 3) = to_bytes.bytes_[2];
                *(_pd + 4) = to_bytes.bytes_[3];
                *(_pd + 5) = to_bytes.bytes_[4];
                *(_pd + 6) = to_bytes.bytes_[5];
                *(_pd + 7) = to_bytes.bytes_[6];
                break;
            case 8:
                *(_pd + 1) = to_bytes.bytes_[0];
                *(_pd + 2) = to_bytes.bytes_[1];
                *(_pd + 3) = to_bytes.bytes_[2];
                *(_pd + 4) = to_bytes.bytes_[3];
                *(_pd + 5) = to_bytes.bytes_[4];
                *(_pd + 6) = to_bytes.bytes_[5];
                *(_pd + 7) = to_bytes.bytes_[6];
                *(_pd + 8) = to_bytes.bytes_[7];
                break;
            default:
                solid_assert(false);
                return nullptr;
            }
            return _pd + sz + 1;
        }
    }
    return nullptr;
}

inline const char* load_compact(const char* _pb, const size_t _sz, uint8_t& _val)
{
    if (_sz != 0) {
        const int8_t* pb = reinterpret_cast<const int8_t*>(_pb);

        if (*pb >= 0) {
            _val = *pb;
            return _pb + 1;
        } else if (_sz >= 2) {
            _val = *(pb + 1);
            return _pb + 2;
        }
    }
    return nullptr;
}

inline const char* load_compact(const char* _pb, const size_t _sz, uint16_t& _val)
{
    static_assert(EndianessE::Native == EndianessE::Little, "Only little endian is supported");
    if (_sz != 0) {
        const int8_t* pb = reinterpret_cast<const int8_t*>(_pb);

        if (*pb >= 0) {
            _val = *pb;
            return _pb + 1;
        } else {
            const size_t sz = -*pb;
            if (_sz >= (sz + 1)) {
                impl::BytesConvertor<uint16_t> from_bytes;
                from_bytes.value_ = 0;
                switch (sz) {
                case 1:
                    from_bytes.bytes_[0] = *(_pb + 1);
                    break;
                case 2:
                    from_bytes.bytes_[0] = *(_pb + 1);
                    from_bytes.bytes_[1] = *(_pb + 2);
                    break;
                default:
                    solid_assert(false);
                    return nullptr;
                }

                _val = from_bytes.value_;
                return _pb + sz + 1;
            }
        }
    }
    return nullptr;
}
inline const char* load_compact(const char* _pb, const size_t _sz, uint32_t& _val)
{
    static_assert(EndianessE::Native == EndianessE::Little, "Only little endian is supported");
    if (_sz != 0) {
        const int8_t* pb = reinterpret_cast<const int8_t*>(_pb);

        if (*pb >= 0) {
            _val = *pb;
            return _pb + 1;
        } else {
            const size_t sz = -*pb;
            if (_sz >= (sz + 1)) {
                impl::BytesConvertor<uint32_t> from_bytes;

                switch (sz) {
                case 1:
                    from_bytes.bytes_[0] = *(_pb + 1);
                    break;
                case 2:
                    from_bytes.bytes_[0] = *(_pb + 1);
                    from_bytes.bytes_[1] = *(_pb + 2);
                    break;
                case 3:
                    from_bytes.bytes_[0] = *(_pb + 1);
                    from_bytes.bytes_[1] = *(_pb + 2);
                    from_bytes.bytes_[2] = *(_pb + 3);
                    break;
                case 4:
                    from_bytes.bytes_[0] = *(_pb + 1);
                    from_bytes.bytes_[1] = *(_pb + 2);
                    from_bytes.bytes_[2] = *(_pb + 3);
                    from_bytes.bytes_[3] = *(_pb + 4);
                    break;
                default:
                    solid_assert(false);
                    return nullptr;
                }

                _val = from_bytes.value_;
                return _pb + sz + 1;
            }
        }
    }
    return nullptr;
}

inline const char* load_compact(const char* _pb, const size_t _sz, uint64_t& _val)
{
    static_assert(EndianessE::Native == EndianessE::Little, "Only little endian is supported");
    if (_sz != 0) {
        const int8_t* pb = reinterpret_cast<const int8_t*>(_pb);

        if (*pb >= 0) {
            _val = *pb;
            return _pb + 1;
        } else {
            const size_t sz = -*pb;
            if (_sz >= (sz + 1)) {
                impl::BytesConvertor<uint64_t> from_bytes;

                switch (sz) {
                case 1:
                    from_bytes.bytes_[0] = *(_pb + 1);
                    break;
                case 2:
                    from_bytes.bytes_[0] = *(_pb + 1);
                    from_bytes.bytes_[1] = *(_pb + 2);
                    break;
                case 3:
                    from_bytes.bytes_[0] = *(_pb + 1);
                    from_bytes.bytes_[1] = *(_pb + 2);
                    from_bytes.bytes_[2] = *(_pb + 3);
                    break;
                case 4:
                    from_bytes.bytes_[0] = *(_pb + 1);
                    from_bytes.bytes_[1] = *(_pb + 2);
                    from_bytes.bytes_[2] = *(_pb + 3);
                    from_bytes.bytes_[3] = *(_pb + 4);
                    break;
                case 5:
                    from_bytes.bytes_[0] = *(_pb + 1);
                    from_bytes.bytes_[1] = *(_pb + 2);
                    from_bytes.bytes_[2] = *(_pb + 3);
                    from_bytes.bytes_[3] = *(_pb + 4);
                    from_bytes.bytes_[4] = *(_pb + 5);
                    break;
                case 6:
                    from_bytes.bytes_[0] = *(_pb + 1);
                    from_bytes.bytes_[1] = *(_pb + 2);
                    from_bytes.bytes_[2] = *(_pb + 3);
                    from_bytes.bytes_[3] = *(_pb + 4);
                    from_bytes.bytes_[4] = *(_pb + 5);
                    from_bytes.bytes_[5] = *(_pb + 6);
                    break;
                case 7:
                    from_bytes.bytes_[0] = *(_pb + 1);
                    from_bytes.bytes_[1] = *(_pb + 2);
                    from_bytes.bytes_[2] = *(_pb + 3);
                    from_bytes.bytes_[3] = *(_pb + 4);
                    from_bytes.bytes_[4] = *(_pb + 5);
                    from_bytes.bytes_[5] = *(_pb + 6);
                    from_bytes.bytes_[6] = *(_pb + 7);
                    break;
                case 8:
                    from_bytes.bytes_[0] = *(_pb + 1);
                    from_bytes.bytes_[1] = *(_pb + 2);
                    from_bytes.bytes_[2] = *(_pb + 3);
                    from_bytes.bytes_[3] = *(_pb + 4);
                    from_bytes.bytes_[4] = *(_pb + 5);
                    from_bytes.bytes_[5] = *(_pb + 6);
                    from_bytes.bytes_[6] = *(_pb + 7);
                    from_bytes.bytes_[7] = *(_pb + 8);
                    break;
                default:
                    solid_assert(false);
                    return nullptr;
                }

                _val = from_bytes.value_;
                return _pb + sz + 1;
            }
        }
    }
    return nullptr;
}

template <class T>
inline size_t compact_size(const T& _v)
{
    if (_v <= static_cast<uint64_t>(std::numeric_limits<int8_t>::max())) {
        return 1;
    } else {
        return max_padded_byte_cout(_v) + 1;
    }
}

inline size_t compact_size(const char* _pb, const size_t _sz)
{
    if (_sz > 0) {
        const int8_t* pb = reinterpret_cast<const int8_t*>(_pb);
        if (*pb < 0) {
            return (-*pb + 1);
        }
        return 1;
    }
    return 0;
}

inline void store_bit_at(uint8_t* _pbeg, const size_t _bit_idx, const bool _opt)
{
    _pbeg += (_bit_idx >> 3);
    const size_t  bit_off = _bit_idx & 7;
    const uint8_t opt     = _opt;
    // clear the bit
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

} // namespace binary
} // namespace v3
} // namespace serialization
} // namespace solid
