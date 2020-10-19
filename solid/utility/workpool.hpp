// solid/utility/workpool_atomic.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once
#include "solid/utility/common.hpp"
#include "solid/utility/workpool_lockfree.hpp"
#include "solid/utility/workpool_locking.hpp"

//#define SOLID_USE_WORKPOOL_MUTEX

namespace solid {

constexpr size_t workpoll_default_node_capacity_bit_count = 10;

#ifdef SOLID_USE_WORKPOOL_MUTEX

template <typename Job, size_t QNBits = workpoll_default_node_capacity_bit_count, typename Base = impl::WorkPoolBase>
using WorkPool = locking::WorkPool<Job, QNBits, Base>;

#else

template <typename Job, size_t QNBits = workpoll_default_node_capacity_bit_count, typename Base = impl::WorkPoolBase>
using WorkPool = lockfree::WorkPool<Job, QNBits, Base>;

#endif

template <class, size_t QNBits = workpoll_default_node_capacity_bit_count, typename Base = impl::WorkPoolBase>
class CallPool;

template <class R, class... ArgTypes, size_t QNBits, typename Base>
class CallPool<R(ArgTypes...), QNBits, Base> {
    using FunctionT = std::function<R(ArgTypes...)>;
    using WorkPoolT = WorkPool<FunctionT, QNBits, Base>;
    WorkPoolT wp_;

public:
    static constexpr size_t node_capacity = WorkPoolT::node_capacity;

    CallPool() {}

    template <typename... Args>
    CallPool(
        const WorkPoolConfiguration& _cfg,
        const size_t                 _start_wkr_cnt,
        Args&&... _args)
        : wp_(
              _cfg,
              _start_wkr_cnt,
              [](FunctionT& _rfnc, Args&&... _args) {
                  _rfnc(std::forward<ArgTypes>(_args)...);
              },
              std::forward<Args>(_args)...)
    {
    }

    template <typename... Args>
    void start(const WorkPoolConfiguration& _cfg, const size_t _start_wkr_cnt, Args... _args)
    {
        wp_.start(
            _cfg, _start_wkr_cnt,
            [](FunctionT& _rfnc, Args&&... _args) {
                _rfnc(std::forward<Args>(_args)...);
            },
            std::forward<Args>(_args)...);
    }

    template <class JT>
    void push(const JT& _jb)
    {
        wp_.push(_jb);
    }

    template <class JT>
    void push(JT&& _jb)
    {
        wp_.push(std::forward<JT>(_jb));
    }

    template <class JT>
    bool tryPush(const JT& _jb)
    {
        return wp_.tryPush(_jb);
    }

    template <class JT>
    bool tryPush(JT&& _jb)
    {
        return wp_.tryPush(std::forward<JT>(_jb));
    }

    void stop()
    {
        wp_.stop();
    }

    void dumpStatistics() const
    {
        wp_.dumpStatistics();
    }
};

} //namespace solid
