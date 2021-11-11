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
#include "solid/utility/function.hpp"
#include "solid/utility/workpool_lockfree.hpp"
#include "solid/utility/workpool_locking.hpp"
#include "solid/utility/workpool_multicast.hpp"

//#define SOLID_USE_LOCKING_WORKPOOL

namespace solid {

constexpr size_t workpoll_default_node_capacity_bit_count = 10;

namespace locking {
template <typename Job, size_t QNBits = workpoll_default_node_capacity_bit_count, typename Base = solid::impl::WorkPoolBase>
using WorkPoolT = locking::WorkPool<Job, QNBits, Base>;
} //namespace locking

namespace lockfree {
template <typename Job, size_t QNBits = workpoll_default_node_capacity_bit_count, typename Base = solid::impl::WorkPoolBase>
using WorkPoolT = lockfree::WorkPool<Job, QNBits, Base>;
} //namespace lockfree

#ifdef SOLID_USE_LOCKING_WORKPOOL

template <typename Job, size_t QNBits = workpoll_default_node_capacity_bit_count, typename Base = solid::impl::WorkPoolBase>
using WorkPool = locking::WorkPool<Job, QNBits, Base>;

template <typename Job>
using DefaultWorkPoolT = locking::WorkPoolT<Job>;

#else

template <typename Job, size_t QNBits = workpoll_default_node_capacity_bit_count, typename Base = solid::impl::WorkPoolBase>
using WorkPool = lockfree::WorkPool<Job, QNBits, Base>;

#endif

template <typename Job>
using DefaultWorkPoolT = WorkPool<Job>;

template <class, size_t FunctionDataSize = function_default_data_size, template <typename> class WorkP = DefaultWorkPoolT>
class CallPool;

template <class... ArgTypes, size_t FunctionDataSize, template <typename> class WP>
class CallPool<void(ArgTypes...), FunctionDataSize, WP> {
    using FunctionT = Function<void(ArgTypes...), FunctionDataSize>;
    using WorkPoolT = WP<FunctionT>;
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
                _rfnc(std::forward<ArgTypes>(_args)...);
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

    const auto& statistic() const
    {
        return wp_.statistic();
    }
};

template <typename Job, typename MCastJob>
using DefaultWorkPoolMulticastT = WorkPoolMulticast<Job, MCastJob>;

template <class, size_t FunctionDataSize = function_default_data_size, template <typename, typename> class WorkP = DefaultWorkPoolMulticastT>
class CallPoolMulticast;

template <class... ArgTypes, size_t FunctionDataSize, template <typename, typename> class WP>
class CallPoolMulticast<void(ArgTypes...), FunctionDataSize, WP> {
    using FunctionT     = Function<void(ArgTypes...), FunctionDataSize>;
    using FunctionPairT = std::pair<FunctionT, FunctionT>;
    using WorkPoolT     = WP<FunctionT, FunctionPairT>;
    WorkPoolT wp_;

public:
    static constexpr size_t node_capacity = WorkPoolT::node_capacity;

    CallPoolMulticast() {}

    template <typename... Args>
    CallPoolMulticast(
        const WorkPoolConfiguration& _cfg,
        Args&&... _args)
        : wp_(
            _cfg,
            [](FunctionT& _rfnc, Args&&... _args) {
                _rfnc(std::forward<ArgTypes>(_args)...);
            },
            [](const FunctionPairT& _rfnc_pair, Args&&... _args) {
                _rfnc_pair.first(std::forward<ArgTypes>(_args)...);
            },
            [](const FunctionPairT& _rfnc_pair, Args&&... _args) {
                _rfnc_pair.second(std::forward<ArgTypes>(_args)...);
            },
            std::forward<Args>(_args)...)
    {
    }

    template <typename... Args>
    void start(const WorkPoolConfiguration& _cfg, Args... _args)
    {
        wp_.start(
            _cfg,
            [](FunctionT& _rfnc, Args&&... _args) {
                _rfnc(std::forward<ArgTypes>(_args)...);
            },
            [](FunctionPairT& _rfnc_pair, Args&&... _args) {
                _rfnc_pair.first(std::forward<ArgTypes>(_args)...);
            },
            [](FunctionPairT& _rfnc_pair, Args&&... _args) {
                _rfnc_pair.second(std::forward<ArgTypes>(_args)...);
            },
            std::forward<Args>(_args)...);
    }

    template <class JT>
    void push(const JT& _jb, const JobType _jb_type = JobType::Asynchronous)
    {
        wp_.push(_jb, _jb_type);
    }

    template <class JT>
    void push(JT&& _jb, const JobType _jb_type = JobType::Asynchronous)
    {
        wp_.push(std::forward<JT>(_jb), _jb_type);
    }

    template <class JT>
    bool tryPush(const JT& _jb, const JobType _jb_type = JobType::Asynchronous)
    {
        return wp_.tryPush(_jb, _jb_type);
    }

    template <class JT>
    bool tryPush(JT&& _jb, const JobType _jb_type = JobType::Asynchronous)
    {
        return wp_.tryPush(std::forward<JT>(_jb), _jb_type);
    }

    template <class JT, class UpdateFnc>
    void pushAll(const JT& _jb, const UpdateFnc& _update_fnc, const JobType _jb_type = JobType::Synchronous)
    {
        wp_.pushAll(FunctionPairT(_jb, _update_fnc), _jb_type);
    }

    template <class JT, class UpdateFnc>
    void pushAll(JT&& _jb, UpdateFnc&& _update_fnc, const JobType _jb_type = JobType::Synchronous)
    {
        wp_.pushAll(FunctionPairT(std::forward<JT>(_jb), std::forward<UpdateFnc>(_update_fnc)), _jb_type);
    }

    void stop()
    {
        wp_.stop();
    }

    const auto& statistic() const
    {
        return wp_.statistic();
    }
};

} //namespace solid
