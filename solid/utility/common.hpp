// solid/utility/common.hpp
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
#include <limits>

namespace solid {

template <typename T>
inline constexpr bool overflow_safe_less(const T& _u1, const T& _u2)
{
    static_assert(std::is_unsigned_v<T>, "Type must be an unsigned_integer");
    constexpr T pivot = (std::numeric_limits<T>::max() << 1);

    if (_u1 < _u2) {
        return (_u2 - _u1) <= pivot;
    } else {
        return (_u1 - _u2) > pivot;
    }
}

template <typename T>
inline constexpr T overflow_safe_max(const T& _u1, const T& _u2)
{
    if (overflow_safe_less(_u1, _u2)) {
        return _u2;
    } else {
        return _u1;
    }
}

inline constexpr uint64_t overflow_safe_max(const uint64_t& _u1, const uint64_t& _u2)
{
    if (overflow_safe_less(_u1, _u2)) {
        return _u2;
    } else {
        return _u1;
    }
}

template <typename T>
inline constexpr T circular_distance(const T& _v, const T& _piv, const T& _max)
{
    if (_v >= _piv) {
        return _v - _piv;
    } else {
        return _max - _piv + _v;
    }
}

inline constexpr size_t padded_size(const size_t _sz, const size_t _pad)
{
    const size_t pad = (_pad - (_sz % _pad)) % _pad;
    return _sz + pad;
}

inline constexpr size_t fast_padded_size(const size_t _sz, const size_t _bitpad)
{
    // return padded_size(_sz, 1<<_bitpad);
    const size_t padv   = static_cast<size_t>(1) << _bitpad;
    const size_t padmsk = padv - 1;
    const size_t pad    = (padv - (_sz & padmsk)) & padmsk;
    return _sz + pad;
}

size_t bit_count(const uint8_t _v);
size_t bit_count(const uint16_t _v);
size_t bit_count(const uint32_t _v);
size_t bit_count(const uint64_t _v);

inline size_t leading_zero_count(uint64_t x)
{
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    x = x | (x >> 8);
    x = x | (x >> 16);
    x = x | (x >> 32);
    return bit_count(~x);
}

inline size_t leading_zero_count(uint32_t x)
{
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    x = x | (x >> 8);
    x = x | (x >> 16);
    return bit_count(~x);
}

inline size_t leading_zero_count(uint16_t x)
{
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    x = x | (x >> 8);
    return bit_count(static_cast<uint16_t>(~x));
}

inline size_t leading_zero_count(uint8_t x)
{
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    return bit_count(static_cast<uint8_t>(~x));
}

inline constexpr void pack(uint32_t& _v, const uint16_t _v1, const uint16_t _v2)
{
    _v = _v2;
    _v <<= 16;
    _v |= _v1;
}

inline constexpr uint32_t pack(const uint16_t _v1, const uint16_t _v2)
{
    uint32_t v = 0;
    pack(v, _v1, _v2);
    return v;
}

inline constexpr void unpack(uint16_t& _v1, uint16_t& _v2, const uint32_t _v)
{
    _v1 = _v & 0xffffUL;
    _v2 = (_v >> 16) & 0xffffUL;
}

extern const uint8_t reverted_chars[];

inline uint32_t bit_revert(const uint32_t _v)
{
    uint32_t r = (((uint32_t)reverted_chars[_v & 0xff]) << 24);
    r |= (((uint32_t)reverted_chars[(_v >> 8) & 0xff]) << 16);
    r |= (((uint32_t)reverted_chars[(_v >> 16) & 0xff]) << 8);
    r |= (((uint32_t)reverted_chars[(_v >> 24) & 0xff]) << 0);
    return r;
}

inline uint64_t bit_revert(const uint64_t _v)
{
    uint64_t r = (((uint64_t)reverted_chars[_v & 0xff]) << 56);
    r |= (((uint64_t)reverted_chars[(_v >> 8) & 0xff]) << 48);
    r |= (((uint64_t)reverted_chars[(_v >> 16) & 0xff]) << 40);
    r |= (((uint64_t)reverted_chars[(_v >> 24) & 0xff]) << 32);

    r |= (((uint64_t)reverted_chars[(_v >> 32) & 0xff]) << 24);
    r |= (((uint64_t)reverted_chars[(_v >> 40) & 0xff]) << 16);
    r |= (((uint64_t)reverted_chars[(_v >> 48) & 0xff]) << 8);
    r |= (((uint64_t)reverted_chars[(_v >> 56) & 0xff]) << 0);
    return r;
}

struct InvalidIndex {
    template <typename SizeT>
    operator SizeT() const
    {
        return (std::numeric_limits<SizeT>::max)();
    }
};

struct InvalidSize {
    template <typename SizeT>
    operator SizeT() const
    {
        return (std::numeric_limits<SizeT>::max)();
    }
};

template <typename SizeT>
constexpr bool operator==(SizeT const& _index, InvalidIndex const& _invalid)
{
    return _index == static_cast<SizeT>(_invalid);
}

template <typename SizeT>
constexpr bool operator!=(SizeT const& _index, InvalidIndex const& _invalid)
{
    return _index != static_cast<SizeT>(_invalid);
}

template <typename SizeT>
constexpr bool operator==(SizeT const& _index, InvalidSize const& _invalid)
{
    return _index == static_cast<SizeT>(_invalid);
}

template <typename SizeT>
constexpr bool operator!=(SizeT const& _index, InvalidSize const& _invalid)
{
    return _index != static_cast<SizeT>(_invalid);
}

template <typename SizeT>
constexpr inline bool is_invalid_index(SizeT const& _index)
{
    return _index == InvalidIndex();
}

template <typename SizeT>
constexpr inline bool is_valid_index(SizeT const& _index)
{
    return _index != InvalidIndex();
}

template <typename SizeT>
constexpr inline bool is_invalid_size(SizeT const& _index)
{
    return _index == InvalidSize();
}

template <typename SizeT>
constexpr inline bool is_valid_size(SizeT const& _index)
{
    return _index != InvalidSize();
}

template <class Enum>
inline constexpr std::underlying_type_t<Enum> to_underlying(const Enum& _val)
{
    return static_cast<std::underlying_type_t<Enum>>(_val);
}

enum struct EndianessE : int {
#ifdef SOLID_ON_WINDOWS
    Little = 0,
    Big    = 1,
#ifdef SOLID_ON_BIG_ENDIAN
    Native = Big
#else
    Native = Little
#endif
#else
    Little = __ORDER_LITTLE_ENDIAN__,
    Big    = __ORDER_BIG_ENDIAN__,
    Native = __BYTE_ORDER__
#endif
};

#ifdef __cpp_concepts
template <class Base, size_t Size = hardware_destructive_interference_size>
class Padded;

template <class Base, size_t Size>
    requires((sizeof(Base) % Size) == 0)
class Padded<Base, Size> : public Base {
};

template <class Base, size_t Size>
class Padded : public Base {
    static constexpr size_t padding_size = (Size - (sizeof(Base) % Size));
    uint8_t                 padding_[padding_size];
};

#else

template <class Base, size_t Size = hardware_destructive_interference_size, class Enabled = void>
class Padded;

template <class Base, size_t Size>
class Padded<Base, Size, typename std::enable_if_t<(sizeof(Base) % Size) == 0>> : public Base {
};

template <class Base, size_t Size, class Enable>
class Padded : public Base {
    static constexpr size_t padding_size = (Size - (sizeof(Base) % Size));
    uint8_t                 padding_[padding_size];
};

#endif

} // namespace solid
