// solid/serialization/src/binarybasic.cpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/serialization/v1/binarybasic.hpp"
#include "solid/system/cassert.hpp"

namespace solid {
namespace serialization {
namespace binary {
namespace cross {
//========================================================================
char* store(char* _pd, const size_t _sz, uint8_t _v)
{
    uint8_t*     pd = reinterpret_cast<uint8_t*>(_pd);
    const size_t sz = max_padded_byte_cout(_v);
    if ((sz + 1) <= _sz) {
        const bool ok = compute_value_with_crc(*pd, static_cast<uint8_t>(sz));
        if (ok) {
            ++pd;
            if (sz) {
                *pd = _v;
            }
            return _pd + sz + 1;
        }
    }
    return nullptr;
}
char* store(char* _pd, const size_t _sz, uint16_t _v)
{
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
                *(pd + 0) = (_v & 0xff);
                _v >>= 8;
                *(pd + 1) = (_v & 0xff);
                break;
            default:
                return nullptr;
            }

            return _pd + sz + 1;
        }
    }
    return nullptr;
}
char* store(char* _pd, const size_t _sz, uint32_t _v)
{
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
                *(pd + 0) = (_v & 0xff);
                _v >>= 8;
                *(pd + 1) = (_v & 0xff);
                break;
            case 3:
                *(pd + 0) = (_v & 0xff);
                _v >>= 8;
                *(pd + 1) = (_v & 0xff);
                _v >>= 8;
                *(pd + 2) = (_v & 0xff);
                break;
            case 4:
                *(pd + 0) = (_v & 0xff);
                _v >>= 8;
                *(pd + 1) = (_v & 0xff);
                _v >>= 8;
                *(pd + 2) = (_v & 0xff);
                _v >>= 8;
                *(pd + 3) = (_v & 0xff);
                break;
            default:
                return nullptr;
            }

            return _pd + sz + 1;
        }
    }
    return nullptr;
}
char* store(char* _pd, const size_t _sz, uint64_t _v)
{
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
                *(pd + 0) = (_v & 0xff);
                _v >>= 8;
                *(pd + 1) = (_v & 0xff);
                break;
            case 3:
                *(pd + 0) = (_v & 0xff);
                _v >>= 8;
                *(pd + 1) = (_v & 0xff);
                _v >>= 8;
                *(pd + 2) = (_v & 0xff);
                break;
            case 4:
                *(pd + 0) = (_v & 0xff);
                _v >>= 8;
                *(pd + 1) = (_v & 0xff);
                _v >>= 8;
                *(pd + 2) = (_v & 0xff);
                _v >>= 8;
                *(pd + 3) = (_v & 0xff);
                break;
            case 5:
                *(pd + 0) = (_v & 0xff);
                _v >>= 8;
                *(pd + 1) = (_v & 0xff);
                _v >>= 8;
                *(pd + 2) = (_v & 0xff);
                _v >>= 8;
                *(pd + 3) = (_v & 0xff);
                _v >>= 8;
                *(pd + 4) = (_v & 0xff);
                break;
            case 6:
                *(pd + 0) = (_v & 0xff);
                _v >>= 8;
                *(pd + 1) = (_v & 0xff);
                _v >>= 8;
                *(pd + 2) = (_v & 0xff);
                _v >>= 8;
                *(pd + 3) = (_v & 0xff);
                _v >>= 8;
                *(pd + 4) = (_v & 0xff);
                _v >>= 8;
                *(pd + 5) = (_v & 0xff);
                break;
            case 7:
                *(pd + 0) = (_v & 0xff);
                _v >>= 8;
                *(pd + 1) = (_v & 0xff);
                _v >>= 8;
                *(pd + 2) = (_v & 0xff);
                _v >>= 8;
                *(pd + 3) = (_v & 0xff);
                _v >>= 8;
                *(pd + 4) = (_v & 0xff);
                _v >>= 8;
                *(pd + 5) = (_v & 0xff);
                _v >>= 8;
                *(pd + 6) = (_v & 0xff);
                break;
            case 8:
                *(pd + 0) = (_v & 0xff);
                _v >>= 8;
                *(pd + 1) = (_v & 0xff);
                _v >>= 8;
                *(pd + 2) = (_v & 0xff);
                _v >>= 8;
                *(pd + 3) = (_v & 0xff);
                _v >>= 8;
                *(pd + 4) = (_v & 0xff);
                _v >>= 8;
                *(pd + 5) = (_v & 0xff);
                _v >>= 8;
                *(pd + 6) = (_v & 0xff);
                _v >>= 8;
                *(pd + 7) = (_v & 0xff);
                break;
            default:
                return nullptr;
            }

            return _pd + sz + 1;
        }
    }
    return nullptr;
}

const char* load(const char* _ps, const size_t _sz, uint8_t& _val)
{
    if (_sz != 0) {
        const uint8_t* ps = reinterpret_cast<const uint8_t*>(_ps);
        uint8_t        v  = *ps;
        const bool     ok = check_value_with_crc(v, v);
        const size_t   sz = v;

        if (ok && (sz + 1) <= _sz) {
            ++ps;

            switch (sz) {
            case 0:
                _val = 0;
                break;
            case 1:
                _val = *ps;
                break;
            default:
                return nullptr;
            }

            return _ps + static_cast<size_t>(v) + 1;
        }
    }
    return nullptr;
}

const char* load(const char* _ps, const size_t _sz, uint16_t& _val)
{
    if (_sz != 0) {
        const uint8_t* ps = reinterpret_cast<const uint8_t*>(_ps);
        uint8_t        v  = *ps;
        const bool     ok = check_value_with_crc(v, v);
        const size_t   sz = v;

        if (ok && (sz + 1) <= _sz) {
            ++ps;

            switch (sz) {
            case 0:
                _val = 0;
                break;
            case 1:
                _val = *ps;
                break;
            case 2:
                _val = *ps;
                _val |= static_cast<uint16_t>(*(ps + 1)) << 8;
                break;
            default:
                return nullptr;
            }
            return _ps + static_cast<size_t>(v) + 1;
        }
    }
    return nullptr;
}

const char* load(const char* _ps, const size_t _sz, uint32_t& _val)
{
    if (_sz != 0) {
        const uint8_t* ps = reinterpret_cast<const uint8_t*>(_ps);
        uint8_t        v  = *ps;
        const bool     ok = check_value_with_crc(v, v);
        const size_t   sz = v;

        if (ok && (sz + 1) <= _sz) {
            ++ps;

            switch (sz) {
            case 0:
                _val = 0;
                break;
            case 1:
                _val = *ps;
                break;
            case 2:
                _val = *ps;
                _val |= static_cast<uint32_t>(*(ps + 1)) << 8;
                break;
            case 3:
                _val = *ps;
                _val |= static_cast<uint32_t>(*(ps + 1)) << 8;
                _val |= static_cast<uint32_t>(*(ps + 2)) << 16;
                break;
            case 4:
                _val = *ps;
                _val |= static_cast<uint32_t>(*(ps + 1)) << 8;
                _val |= static_cast<uint32_t>(*(ps + 2)) << 16;
                _val |= static_cast<uint32_t>(*(ps + 3)) << 24;
                break;
            default:
                return nullptr;
            }
            return _ps + static_cast<size_t>(v) + 1;
        }
    }
    return nullptr;
}

const char* load(const char* _ps, const size_t _sz, uint64_t& _val)
{
    if (_sz != 0) {
        const uint8_t* ps = reinterpret_cast<const uint8_t*>(_ps);
        uint8_t        v  = *ps;
        const bool     ok = check_value_with_crc(v, v);
        const size_t   sz = v;

        if (ok && (sz + 1) <= _sz) {
            ++ps;

            switch (sz) {
            case 0:
                _val = 0;
                break;
            case 1:
                _val = *ps;
                break;
            case 2:
                _val = *ps;
                _val |= static_cast<uint64_t>(*(ps + 1)) << 8;
                break;
            case 3:
                _val = *ps;
                _val |= static_cast<uint64_t>(*(ps + 1)) << 8;
                _val |= static_cast<uint64_t>(*(ps + 2)) << 16;
                break;
            case 4:
                _val = *ps;
                _val |= static_cast<uint64_t>(*(ps + 1)) << 8;
                _val |= static_cast<uint64_t>(*(ps + 2)) << 16;
                _val |= static_cast<uint64_t>(*(ps + 3)) << 24;
                break;
            case 5:
                _val = *ps;
                _val |= static_cast<uint64_t>(*(ps + 1)) << 8;
                _val |= static_cast<uint64_t>(*(ps + 2)) << 16;
                _val |= static_cast<uint64_t>(*(ps + 3)) << 24;
                _val |= static_cast<uint64_t>(*(ps + 4)) << 32;
                break;
            case 6:
                _val = *ps;
                _val |= static_cast<uint64_t>(*(ps + 1)) << 8;
                _val |= static_cast<uint64_t>(*(ps + 2)) << 16;
                _val |= static_cast<uint64_t>(*(ps + 3)) << 24;
                _val |= static_cast<uint64_t>(*(ps + 4)) << 32;
                _val |= static_cast<uint64_t>(*(ps + 5)) << 40;
                break;
            case 7:
                _val = *ps;
                _val |= static_cast<uint64_t>(*(ps + 1)) << 8;
                _val |= static_cast<uint64_t>(*(ps + 2)) << 16;
                _val |= static_cast<uint64_t>(*(ps + 3)) << 24;
                _val |= static_cast<uint64_t>(*(ps + 4)) << 32;
                _val |= static_cast<uint64_t>(*(ps + 5)) << 40;
                _val |= static_cast<uint64_t>(*(ps + 6)) << 48;
                break;
            case 8:
                _val = *ps;
                _val |= static_cast<uint64_t>(*(ps + 1)) << 8;
                _val |= static_cast<uint64_t>(*(ps + 2)) << 16;
                _val |= static_cast<uint64_t>(*(ps + 3)) << 24;
                _val |= static_cast<uint64_t>(*(ps + 4)) << 32;
                _val |= static_cast<uint64_t>(*(ps + 5)) << 40;
                _val |= static_cast<uint64_t>(*(ps + 6)) << 48;
                _val |= static_cast<uint64_t>(*(ps + 7)) << 56;
                break;
            default:
                return nullptr;
            }
            return _ps + static_cast<size_t>(v) + 1;
        }
    }
    return nullptr;
}
//========================================================================
} //namespace cross
} //namespace binary
} //namespace serialization
} //namespace solid
