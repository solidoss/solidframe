// solid/serialization/v2/src/binarybasic.cpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/serialization/v2/binarybasic.hpp"
#include "solid/serialization/v2/binarybase.hpp"
#include "solid/system/cassert.hpp"

namespace solid {
namespace serialization {
namespace v2 {
namespace binary {
namespace cross {
//========================================================================
char* store_with_check(char* _pd, const size_t _sz, uint8_t _v)
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
char* store_with_check(char* _pd, const size_t _sz, uint16_t _v)
{
#ifdef SOLID_ON_BIG_ENDIAN
    _v = swap_bites(_v);
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
            default:
                return nullptr;
            }

            return _pd + sz + 1;
        }
    }
    return nullptr;
}

const char* load_with_check(const char* _ps, const size_t _sz, uint8_t& _val)
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

const char* load_with_check(const char* _ps, const size_t _sz, uint16_t& _val)
{
    if (_sz != 0) {
        const uint8_t* ps  = reinterpret_cast<const uint8_t*>(_ps);
        uint8_t        vsz = *ps;
        const bool     ok  = check_value_with_crc(vsz, vsz);
        const size_t   sz  = vsz;
        uint16_t       v   = 0;

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
                v |= static_cast<uint16_t>(*(ps + 1)) << 8;
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

//========================================================================
} //namespace cross
} //namespace binary
} //namespace v2
} //namespace serialization
} //namespace solid
