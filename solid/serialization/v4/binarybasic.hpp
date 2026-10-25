// solid/serialization/v4/binarybasic.hpp
//
// Copyright (c) 2026 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

// Wire primitives of the binary serialization - identical to v3:
//  - integers are stored on sizeof(T) bytes, little endian;
//  - "compacted" unsigned values (sizes, variant indexes, with_compacted
//    integers) use one byte for values <= 127, the single byte 0x80 for
//    UINT64_MAX, otherwise a byte holding -N followed by the N significant
//    bytes of the value, little endian. A compacted integer with all bits set
//    in its type (unsigned max, signed -1) is stored as UINT64_MAX and loaded
//    as all bits set in the reader's type, whatever its width;
//  - bit sequences are packed LSB first.
namespace solid::serialization::inline v4::binary {

static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big, "mixed endian is not supported");

inline constexpr size_t max_compacted_size = 1 + sizeof(uint64_t);

// First byte (and the whole encoding) of a compacted UINT64_MAX.
inline constexpr int8_t compact_max_marker = std::numeric_limits<int8_t>::min();

template <std::integral T>
inline constexpr T all_bits_set = static_cast<T>(~T{});

// The value a compacted integer is encoded as: all bits set in T becomes
// UINT64_MAX (the marker), the rest is widened (signed: sign extended).
template <std::integral T>
constexpr uint64_t to_compact(const T _v) noexcept
{
    return _v == all_bits_set<T> ? std::numeric_limits<uint64_t>::max() : static_cast<uint64_t>(_v);
}

// Narrows a decoded compacted value to T: UINT64_MAX becomes all bits set in
// T; false if the value does not fit T.
template <std::integral T>
constexpr bool from_compact(const uint64_t _v, T& _rv) noexcept
{
    if (_v == std::numeric_limits<uint64_t>::max()) {
        _rv = all_bits_set<T>;
        return true;
    }
    const auto v = static_cast<T>(_v);
    if (static_cast<uint64_t>(v) != _v) {
        return false;
    }
    _rv = v;
    return true;
}

template <std::integral T>
inline char* store(char* _pd, const T _v) noexcept
{
    if constexpr (std::endian::native == std::endian::little || sizeof(T) == 1) {
        std::memcpy(_pd, &_v, sizeof(T));
    } else {
        const auto v = std::byteswap(_v);
        std::memcpy(_pd, &v, sizeof(T));
    }
    return _pd + sizeof(T);
}

template <std::integral T>
inline const char* load(const char* _ps, T& _rv) noexcept
{
    std::memcpy(&_rv, _ps, sizeof(T));
    if constexpr (std::endian::native == std::endian::big && sizeof(T) != 1) {
        _rv = std::byteswap(_rv);
    }
    return _ps + sizeof(T);
}

// Number of bytes store_compact needs for _v.
constexpr size_t compact_size(const uint64_t _v) noexcept
{
    if (_v <= 127 || _v == std::numeric_limits<uint64_t>::max()) {
        return 1;
    }
    return 1 + ((static_cast<size_t>(std::bit_width(_v)) + 7) >> 3);
}

// Number of bytes of the compacted value starting at _pb, as told by its
// first byte; 0 if _sz == 0.
inline size_t compact_size(const char* _pb, const size_t _sz) noexcept
{
    if (_sz == 0) {
        return 0;
    }
    const auto c = static_cast<int8_t>(*_pb);
    return c >= 0 || c == compact_max_marker ? 1 : 1 + static_cast<size_t>(-c);
}

// Encodes _v at _pd (which must have max_compacted_size bytes available);
// returns the end of the encoded value.
inline char* store_compact(char* _pd, const uint64_t _v) noexcept
{
    if (_v <= 127) {
        *_pd = static_cast<char>(_v);
        return _pd + 1;
    }
    if (_v == std::numeric_limits<uint64_t>::max()) {
        *_pd = static_cast<char>(compact_max_marker);
        return _pd + 1;
    }
    const size_t sz = compact_size(_v) - 1;
    *_pd            = static_cast<char>(-static_cast<int8_t>(sz));
    if constexpr (std::endian::native == std::endian::little) {
        std::memcpy(_pd + 1, &_v, sz);
    } else {
        for (size_t i = 0; i < sz; ++i) {
            _pd[1 + i] = static_cast<char>(_v >> (8 * i));
        }
    }
    return _pd + 1 + sz;
}

// v3 compatible bounded overload: nullptr if _sz is too small.
template <std::unsigned_integral T>
inline char* store_compact(char* _pd, const size_t _sz, const T _v) noexcept
{
    if (compact_size(to_compact(_v)) > _sz) {
        return nullptr;
    }
    return store_compact(_pd, to_compact(_v));
}

// Decodes a compacted value from [_pb, _pb + _sz) into _rv (see from_compact).
// Returns the end of the value, nullptr if not enough bytes are available, or
// _pb itself if the encoding is invalid (more than 8 bytes or value does not
// fit T).
template <std::unsigned_integral T>
inline const char* load_compact(const char* _pb, const size_t _sz, T& _rv) noexcept
{
    if (_sz == 0) {
        return nullptr;
    }
    const auto c = static_cast<int8_t>(*_pb);
    if (c >= 0) {
        _rv = static_cast<T>(c);
        return _pb + 1;
    }
    if (c == compact_max_marker) {
        _rv = all_bits_set<T>;
        return _pb + 1;
    }
    const auto sz = static_cast<size_t>(-c);
    if (sz > sizeof(uint64_t)) {
        return _pb;
    }
    if (_sz < sz + 1) {
        return nullptr;
    }
    uint64_t v = 0;
    if constexpr (std::endian::native == std::endian::little) {
        std::memcpy(&v, _pb + 1, sz);
    } else {
        for (size_t i = 0; i < sz; ++i) {
            v |= static_cast<uint64_t>(static_cast<uint8_t>(_pb[1 + i])) << (8 * i);
        }
    }
    if (!from_compact(v, _rv)) {
        return _pb;
    }
    return _pb + 1 + sz;
}

inline void store_bit_at(uint8_t* _pbeg, const size_t _bit_idx, const bool _opt) noexcept
{
    _pbeg += (_bit_idx >> 3);
    const auto bit_off = static_cast<unsigned>(_bit_idx & 7);
    *_pbeg             = static_cast<uint8_t>((*_pbeg & ~(1u << bit_off)) | (static_cast<unsigned>(_opt) << bit_off));
}

inline bool load_bit_from(const uint8_t* _pbeg, const size_t _bit_idx) noexcept
{
    return ((_pbeg[_bit_idx >> 3] >> (_bit_idx & 7)) & 1) != 0;
}

} // namespace solid::serialization::inline v4::binary
