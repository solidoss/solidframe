// solid/frame/common.hpp
//
// Copyright (c) 2007, 2008, 2013, 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <ostream>
#include <utility>

#include "solid/system/convertors.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/function.hpp"

namespace solid {
namespace frame {

using IndexT  = uint64_t;
using UniqueT = uint32_t;

struct UniqueId {
    IndexT  index;
    UniqueT unique;

    static UniqueId invalid()
    {
        return UniqueId();
    }

    UniqueId(
        IndexT const& _idx = InvalidIndex(),
        UniqueT       _unq = InvalidIndex())
        : index(_idx)
        , unique(_unq)
    {
    }

    bool isInvalid() const
    {
        return index == InvalidIndex();
    }
    bool isValid() const
    {
        return !isInvalid();
    }

    bool operator==(UniqueId const& _ruid) const
    {
        return _ruid.index == this->index && _ruid.unique == this->unique;
    }
    bool operator!=(UniqueId const& _ruid) const
    {
        return _ruid.index != this->index || _ruid.unique != this->unique;
    }
    void clear()
    {
        index  = InvalidIndex();
        unique = InvalidIndex();
    }
};

using ObjectIdT = UniqueId;

std::ostream& operator<<(std::ostream& _ros, UniqueId const& _uid);

enum {
    IndexBitCount = sizeof(IndexT) * 8,
};

inline bool is_valid_index(const IndexT& _idx)
{
    return _idx != static_cast<frame::IndexT>(-1);
}

inline bool is_invalid_index(const IndexT& _idx)
{
    return _idx == static_cast<frame::IndexT>(-1);
}

template <typename T>
T unite_index(T _hi, const T& _lo, const int _hibitcnt);

template <typename T>
void split_index(T& _hi, T& _lo, const int _hibitcnt, const T _v);

template <>
inline uint32_t unite_index<uint32_t>(uint32_t _hi, const uint32_t& _lo, const int /*_hibitcnt*/)
{
    return bit_revert(_hi) | _lo;
}

template <>
inline uint64_t unite_index<uint64_t>(uint64_t _hi, const uint64_t& _lo, const int /*_hibitcnt*/)
{
    return bit_revert(_hi) | _lo;
}

template <>
inline void split_index<uint32_t>(uint32_t& _hi, uint32_t& _lo, const int _hibitcnt, const uint32_t _v)
{
    const uint32_t lomsk = bitsToMask32(32 - _hibitcnt); //(1 << (32 - _hibitcnt)) - 1;
    _lo                  = _v & lomsk;
    _hi                  = bit_revert(_v & (~lomsk));
}

template <>
inline void split_index<uint64_t>(uint64_t& _hi, uint64_t& _lo, const int _hibitcnt, const uint64_t _v)
{
    const uint64_t lomsk = bitsToMask64(64 - _hibitcnt); //(1ULL << (64 - _hibitcnt)) - 1;
    _lo                  = _v & lomsk;
    _hi                  = bit_revert(_v & (~lomsk));
}

template <class V>
typename V::value_type& safe_at(V& _v, uint _pos)
{
    if (_pos < _v.size()) {
        return _v[_pos];
    } else {
        _v.resize(_pos + 1);
        return _v[_pos];
    }
}

template <class V>
IndexT smart_resize(V& _rv, const IndexT& _rby)
{
    _rv.resize(((_rv.size() / _rby) + 1) * _rby);
    return _rv.size();
}

template <class V>
IndexT fast_smart_resize(V& _rv, const size_t _bitby)
{
    _rv.resize(((_rv.size() >> _bitby) + 1) << _bitby);
    return _rv.size();
}

enum ReactorEventsE {
    ReactorEventNone  = 0,
    ReactorEventError = 8,
    ReactorEventClear = 128,
    ReactorEventInit  = 256,
    ReactorEventTimer = 512,
};

enum ReactorWaitRequestsE {
    ReactorWaitNone = 0,
    //Add above!
    ReactorWaitError
};

} //namespace frame
} //namespace solid
