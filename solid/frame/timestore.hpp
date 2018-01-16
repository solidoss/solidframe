// solid/frame/timestore.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/nanotime.hpp"
#include <vector>

namespace solid {
namespace frame {

template <typename V>
class TimeStore {
public:
    typedef V ValueT;

    TimeStore(const size_t _cp = 0)
    {
        tv.reserve(_cp);
        mint = NanoTime::maximum;
    }
    ~TimeStore() {}

    size_t size() const
    {
        return tv.size();
    }

    size_t push(NanoTime const& _rt, ValueT const& _rv)
    {
        SOLID_ASSERT(_rv != InvalidIndex());
        const size_t rv = tv.size();
        tv.push_back(TimePairT(_rt, _rv));
        if (_rt < mint) {
            mint = _rt;
        }
        return rv;
    }

    template <typename F>
    void pop(const size_t _idx, F const& _rf)
    {
        const size_t oldidx = tv.size() - 1;
        tv[_idx]            = tv.back();
        SOLID_ASSERT(!tv.empty());
        tv.pop_back();
        if (_idx < tv.size()) {
            _rf(tv[_idx].second, _idx, oldidx);
        }
    }

    ValueT change(const size_t _idx, NanoTime const& _rt)
    {
        tv[_idx].first = _rt;
        if (_rt < mint) {
            mint = _rt;
        }
        return tv[_idx].second;
    }

    template <typename F1, typename F2>
    void pop(NanoTime const& _rt, F1 const& _rf1, F2 const& _rf2)
    {

        NanoTime crtmin = NanoTime::maximum;
        for (size_t i = 0; i < tv.size();) {
            TimePairT const& rtp = tv[i];
            if (rtp.first > _rt) {
                ++i;
                if (rtp.first < crtmin) {
                    crtmin = rtp.first;
                }
            } else {
                ValueT       v      = rtp.second;
                const size_t oldidx = tv.size() - 1;
                tv[i]               = tv.back();
                tv.pop_back();
                if (i < tv.size()) {
                    _rf2(tv[i].second, i, oldidx);
                }

                _rf1(i, v);
            }
        }

        mint = crtmin;
    }
    NanoTime const& next() const
    {
        return mint;
    }

private:
    typedef std::pair<NanoTime, ValueT> TimePairT;
    typedef std::vector<TimePairT>      TimeVectorT;
    TimeVectorT                         tv;
    NanoTime                            mint;
};

} //namespace frame
} //namespace solid
