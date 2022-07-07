// solid/frame/mprpc/mprpccompression_snappy.hpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <cstring>

#include "snappy.h"
#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol.hpp"

namespace solid {
namespace frame {
namespace mprpc {
namespace snappy {

struct Engine {
    const size_t buff_threshold;
    const size_t diff_threshold;

    Engine(size_t _buff_threshold, size_t _diff_threshold)
        : buff_threshold(_buff_threshold)
        , diff_threshold(_diff_threshold)
    {
    }

    // compression:
    size_t operator()(char* _piobuf, size_t _bufsz, ErrorConditionT&)
    {

        if (_bufsz > buff_threshold) {

            char tmpbuf[Protocol::MaxPacketDataSize];

            size_t len = 0;

            ::snappy::RawCompress(_piobuf, _bufsz, tmpbuf, &len);

            if (_bufsz <= len || (_bufsz - len) < diff_threshold) {
                return 0; // compression not eficient
            }

            memcpy(_piobuf, tmpbuf, len);
            return len;
        } else {
            // buffer too small to compress
            return 0;
        }
    }

    // decompression:
    size_t operator()(char* _pto, const char* _pfrom, size_t _from_sz, ErrorConditionT& _rerror)
    {
        size_t newlen = 0;
        if (::snappy::GetUncompressedLength(_pfrom, _from_sz, &newlen)) {
        } else {
            _rerror = error_compression_engine;
            return 0;
        }

        if (::snappy::RawUncompress(_pfrom, _from_sz, _pto)) {
        } else {
            _rerror = error_compression_engine;
            return 0;
        }
        return newlen;
    }
};

inline void setup(mprpc::Configuration& _rcfg, size_t _buff_threshold = 1024, size_t _diff_threshold = 32)
{
    _rcfg.reader.decompress_fnc       = Engine(_buff_threshold, _diff_threshold);
    _rcfg.writer.inplace_compress_fnc = Engine(_buff_threshold, _diff_threshold);
}

} // namespace snappy
} // namespace mprpc
} // namespace frame
} // namespace solid
