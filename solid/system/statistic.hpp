// solid/system/statistic.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/common.hpp"
#include <atomic>
#include <ostream>

namespace solid {

struct Statistic {
    virtual ~Statistic();
    virtual std::ostream& print(std::ostream& _ros) const;
};

inline std::ostream& operator<<(std::ostream& _ros, const Statistic& _rs)
{
    return _rs.print(_ros);
}

template <class T>
inline void store_max(std::atomic<T>& _rv, const T _nv)
{
    T v = _rv;
    while (v < _nv && !_rv.compare_exchange_weak(v, _nv)) {
    }
}

template <class T>
inline void store_min(std::atomic<T>& _rv, const T _nv)
{
    T v = _rv;
    while (v > _nv && !_rv.compare_exchange_weak(v, _nv)) {
    }
}

} //namespace solid

#ifdef SOLID_HAS_STATISTICS

#define solid_statistic_add(v, by) (v) += (by)
#define solid_statistic_inc(v) ++(v)
#define solid_statistic_max(v, nv) solid::store_max((v), (nv))
#define solid_statistic_min(v, nv) solid::store_min((v), (nv))

#else

#define solid_statistic_add(v, by) (void*)by
#define solid_statistic_inc(v)
#define solid_statistic_max(v, nv) (void*)nv
#define solid_statistic_min(v, nv) (void*)nv

#endif
