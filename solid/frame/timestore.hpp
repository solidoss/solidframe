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

#include <array>
#include <deque>
#include <vector>

#include "solid/system/cassert.hpp"
#include "solid/system/log.hpp"
#include "solid/system/nanotime.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/innerlist.hpp"

namespace solid {
namespace frame {

namespace time_store_impl {
enum struct LinkE : size_t {
    Free = 0,
    // add above
    Count,
};

enum struct IntervalE : size_t {
    Second,
    Minute,
    Hour,
    Other,
    // add above
    Count
};

inline IntervalE dispatch(const NanoTime& _now, const NanoTime& _expiry)
{
    if (_expiry.seconds() < _now.seconds() || _now.seconds() == _expiry.seconds() || (_now.seconds() + 1) == _expiry.seconds()) {
        return IntervalE::Second;
    } else if (_expiry.seconds() <= (_now.seconds() + 60)) {
        return IntervalE::Minute;
    } else if (_expiry.seconds() <= (_now.seconds() + 3600)) {
        return IntervalE::Hour;
    }
    return IntervalE::Other;
}

} // namespace time_store_impl

class TimeStore {
    struct ProxyNode : inner::Node<to_underlying(time_store_impl::LinkE::Count)> {
        size_t                     value_          = InvalidIndex{};
        size_t                     internal_index_ = InvalidIndex{};
        time_store_impl::IntervalE interval_       = time_store_impl::IntervalE::Count;

        void clear()
        {
            value_          = InvalidIndex{};
            internal_index_ = InvalidIndex{};
            interval_       = time_store_impl::IntervalE::Count;
        }

        bool empty() const
        {
            return interval_ == time_store_impl::IntervalE::Count;
        }
    };

    struct Value {
        NanoTime expiry_      = NanoTime::max();
        size_t   proxy_index_ = InvalidIndex{};

        Value() = default;
        Value(const NanoTime& _expiry, const size_t _proxy_index)
            : expiry_(_expiry)
            , proxy_index_(_proxy_index)
        {
        }
    };

    using ValueVectorT = std::vector<Value>;

    struct Interval {
        NanoTime     min_expiry_ = NanoTime::max();
        ValueVectorT values_;
    };

    using ProxyNodesT    = std::deque<ProxyNode>;
    using ProxyFreeListT = inner::List<ProxyNodesT, to_underlying(time_store_impl::LinkE::Free)>;
    using IntervalsT     = std::array<Interval, to_underlying(time_store_impl::IntervalE::Count)>;

    NanoTime       min_expiry_ = NanoTime::max();
    ProxyNodesT    proxy_nodes_;
    ProxyFreeListT proxy_free_list_;
    IntervalsT     intervals_;

public:
    TimeStore(const size_t _capacity = 0);

    size_t size() const;

    bool empty() const;

    size_t push(const NanoTime& _now, const NanoTime& _expiry, const size_t _value);

    void update(const size_t _proxy_index, const NanoTime& _now, const NanoTime& _expiry);

    void pop(const size_t _index);

    template <class Fnc>
    size_t pop(const NanoTime& _now, Fnc&& _fnc);

    const NanoTime& expiry() const
    {
        return min_expiry_;
    }

private:
    Interval& interval(const time_store_impl::IntervalE _iv)
    {
        return intervals_[to_underlying(_iv)];
    }
    template <class Fnc>
    size_t doPop(const time_store_impl::IntervalE _iv, const NanoTime& _now, Fnc&& _fnc);
};

inline TimeStore::TimeStore(const size_t _capacity)
    : proxy_free_list_(proxy_nodes_)
{
    for (auto& iv : intervals_) {
        iv.values_.reserve(_capacity);
    }
}

inline size_t TimeStore::size() const
{
    return proxy_nodes_.size() - proxy_free_list_.size();
}

inline bool TimeStore::empty() const
{
    return size() == 0;
}

inline size_t TimeStore::push(const NanoTime& _now, const NanoTime& _expiry, const size_t _value)
{
    size_t proxy_index = InvalidIndex{};
    if (!proxy_free_list_.empty()) {
        proxy_index = proxy_free_list_.popBack();
    } else {
        proxy_index = proxy_nodes_.size();
        proxy_nodes_.emplace_back();
    }

    auto& rnode     = proxy_nodes_[proxy_index];
    rnode.value_    = _value;
    rnode.interval_ = time_store_impl::dispatch(_now, _expiry);

    auto& rinterval       = interval(rnode.interval_);
    rnode.internal_index_ = rinterval.values_.size();
    rinterval.values_.emplace_back(_expiry, proxy_index);

    solid_dbg(generic_logger, Verbose, " interval = " << to_underlying(rnode.interval_) << " internal index =" << rnode.internal_index_ << " " << _expiry);

    if (_expiry < rinterval.min_expiry_) {
        rinterval.min_expiry_ = _expiry;
    }

    if (_expiry < min_expiry_) {
        min_expiry_ = _expiry;
    }

    return proxy_index;
}

inline void TimeStore::update(const size_t _proxy_index, const NanoTime& _now, const NanoTime& _expiry)
{
    solid_assert(_proxy_index < proxy_nodes_.size() && !proxy_nodes_[_proxy_index].empty());
    auto& rnode = proxy_nodes_[_proxy_index];

    const auto new_interval = time_store_impl::dispatch(_now, _expiry);

    if (new_interval == rnode.interval_) {
        solid_dbg(generic_logger, Verbose, " interval = " << to_underlying(rnode.interval_) << " internal index =" << rnode.internal_index_);
        auto& rinterval = interval(rnode.interval_);
        solid_assert(rinterval.values_.size() > rnode.internal_index_);
        rinterval.values_[rnode.internal_index_].expiry_ = _expiry;
        if (_expiry < rinterval.min_expiry_) {
            rinterval.min_expiry_ = _expiry;
        }
        if (_expiry < min_expiry_) {
            min_expiry_ = _expiry;
        }
    } else {
        solid_dbg(generic_logger, Verbose, "from iv = " << to_underlying(rnode.interval_) << " to iv = " << to_underlying(new_interval) << " internal index =" << rnode.internal_index_);
        {
            auto& rinterval                                                                     = interval(rnode.interval_);
            rinterval.values_[rnode.internal_index_]                                            = rinterval.values_.back();
            proxy_nodes_[rinterval.values_[rnode.internal_index_].proxy_index_].internal_index_ = rnode.internal_index_;
            rinterval.values_.pop_back();
            if (rinterval.values_.empty()) {
                rinterval.min_expiry_ = NanoTime::max();
            }
        }
        {
            rnode.interval_       = new_interval;
            auto& rinterval       = interval(rnode.interval_);
            rnode.internal_index_ = rinterval.values_.size();
            rinterval.values_.emplace_back(_expiry, _proxy_index);
            if (_expiry < rinterval.min_expiry_) {
                rinterval.min_expiry_ = _expiry;
            }

            if (_expiry < min_expiry_) {
                min_expiry_ = _expiry;
            }
        }
    }
}

inline void TimeStore::pop(const size_t _proxy_index)
{
    solid_assert(_proxy_index < proxy_nodes_.size() && !proxy_nodes_[_proxy_index].empty());
    auto& rnode                                                                         = proxy_nodes_[_proxy_index];
    auto& rinterval                                                                     = interval(rnode.interval_);
    rinterval.values_[rnode.internal_index_]                                            = rinterval.values_.back();
    proxy_nodes_[rinterval.values_[rnode.internal_index_].proxy_index_].internal_index_ = rnode.internal_index_;
    rinterval.values_.pop_back();
    if (rinterval.values_.empty()) {
        rinterval.min_expiry_ = NanoTime::max();
    }
    rnode.clear();
    proxy_free_list_.pushBack(_proxy_index);
}

template <class Fnc>
size_t TimeStore::doPop(const time_store_impl::IntervalE _iv, const NanoTime& _now, Fnc&& _fnc)
{
    using namespace time_store_impl;
    auto&  rinterval  = interval(_iv);
    size_t count      = 0;
    auto   min_expiry = NanoTime::max();

    rinterval.min_expiry_ = min_expiry;

    for (size_t i = 0; i < rinterval.values_.size();) {
        auto& rval = rinterval.values_[i];
        if (_now < rval.expiry_) {
            ++i;
            if (rval.expiry_ < min_expiry) {
                min_expiry = rval.expiry_;
            }
            continue;
        } else {
            const auto proxy_index = rval.proxy_index_;
            auto&      rnode       = proxy_nodes_[proxy_index];
            const auto value       = rnode.value_;
            const auto expiry      = rval.expiry_;
            solid_assert(rnode.internal_index_ == i);

            rval = rinterval.values_.back();

            proxy_nodes_[rval.proxy_index_].internal_index_ = i;
            rnode.clear();
            proxy_free_list_.pushBack(proxy_index);

            rinterval.values_.pop_back();

            _fnc(value, expiry, proxy_index);
            ++count;
        }
    }

    if (min_expiry < rinterval.min_expiry_) {
        rinterval.min_expiry_ = min_expiry;
    }
    return count;
}

template <class Fnc>
inline size_t TimeStore::pop(const NanoTime& _now, Fnc&& _fnc)
{
    using namespace time_store_impl;
    size_t count = 0;
    if (_now < expiry()) {
        return count;
    }

    if (!(_now < interval(IntervalE::Second).min_expiry_)) {
        count += doPop(IntervalE::Second, _now, _fnc);
    }
    if (!(_now < interval(IntervalE::Minute).min_expiry_)) {
        count += doPop(IntervalE::Minute, _now, _fnc);
    }
    if (!(_now < interval(IntervalE::Hour).min_expiry_)) {
        count += doPop(IntervalE::Hour, _now, _fnc);
    }
    if (!(_now < interval(IntervalE::Other).min_expiry_)) {
        count += doPop(IntervalE::Other, _now, _fnc);
    }
    min_expiry_ = std::min(
        interval(IntervalE::Second).min_expiry_,
        std::min(
            interval(IntervalE::Minute).min_expiry_,
            std::min(
                interval(IntervalE::Hour).min_expiry_,
                interval(IntervalE::Other).min_expiry_)));
    return count;
}

} // namespace frame
} // namespace solid
