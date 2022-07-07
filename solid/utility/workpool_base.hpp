// solid/utility/workpool_base.hpp
//
// Copyright (c) 2020 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/queue_lockfree.hpp"
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace solid {

extern const LoggerT workpool_logger;
//-----------------------------------------------------------------------------
constexpr size_t workpool_default_node_capacity_bit_count = 10;
//-----------------------------------------------------------------------------

struct WorkPoolConfiguration {
    size_t                max_worker_count_;
    size_t                max_job_queue_size_;
    std::function<void()> on_thread_start_fnc_;
    std::function<void()> on_thread_stop_fnc_;

    WorkPoolConfiguration(
        const size_t            _max_worker_count    = std::thread::hardware_concurrency(),
        const size_t            _max_job_queue_size  = std::numeric_limits<size_t>::max(),
        std::function<void()>&& _on_thread_start_fnc = []() {},
        std::function<void()>&& _on_thread_stop_fnc  = []() {})
        : max_worker_count_(_max_worker_count)
        , max_job_queue_size_(_max_job_queue_size == 0 ? std::numeric_limits<size_t>::max() : _max_job_queue_size)
        , on_thread_start_fnc_(std::move(_on_thread_start_fnc))
        , on_thread_stop_fnc_(std::move(_on_thread_stop_fnc))
    {
    }
};

namespace impl {

class WorkPoolBase : NonCopyable {
protected:
    using QueueBase = lockfree::impl::QueueBase;

    WorkPoolConfiguration config_;

    template <class Predicate>
    void wait(std::condition_variable& _rcv, std::unique_lock<std::mutex>& _rlock, Predicate _predicate)
    {
        _rcv.wait(_rlock, _predicate);
    }

    void wait(std::condition_variable& _rcv, std::unique_lock<std::mutex>& _rlock)
    {
        _rcv.wait(_rlock);
    }
};

template <size_t WaitSeconds>
class StressTestQueueBase : NonCopyable {
protected:
    template <class Predicate>
    void wait(std::condition_variable& _rcv, std::unique_lock<std::mutex>& _rlock, Predicate _predicate)
    {
        solid_check_log(_rcv.wait_for(_rlock, std::chrono::seconds(WaitSeconds), _predicate), workpool_logger, "condition locked waited " << WaitSeconds << " seconds");
    }
};

template <size_t WaitSeconds = 300>
class StressTestWorkPoolBase : NonCopyable {
protected:
    using QueueBase = StressTestQueueBase<WaitSeconds>;
    WorkPoolConfiguration config_;

    template <class Predicate>
    void wait(std::condition_variable& _rcv, std::unique_lock<std::mutex>& _rlock, Predicate _predicate)
    {
        solid_check_log(_rcv.wait_for(_rlock, std::chrono::seconds(WaitSeconds), _predicate), workpool_logger, "condition locked waited " << WaitSeconds << " seconds");
    }

    void wait(std::condition_variable& _rcv, std::unique_lock<std::mutex>& _rlock)
    {
        solid_check_log(_rcv.wait_for(_rlock, std::chrono::seconds(WaitSeconds)) != std::cv_status::timeout, workpool_logger, "condition locked waited " << WaitSeconds << " seconds");
    }
};

} // namespace impl

template <class Job, class MCast, size_t FunctionDataSize, template <typename, typename> class WP>
class CallPool;

template <class... ArgTypes, size_t FunctionDataSize, template <typename, typename> class WP>
class CallPool<void(ArgTypes...), void, FunctionDataSize, WP> {
    using FunctionT = Function<void(ArgTypes...), FunctionDataSize>;
    using WorkPoolT = WP<FunctionT, void>;
    WorkPoolT wp_;

public:
    static constexpr size_t node_capacity = WorkPoolT::node_capacity;

    CallPool() {}

    template <typename... Args>
    CallPool(
        const WorkPoolConfiguration& _cfg,
        Args&&... _args)
        : wp_(
            _cfg,
            [](FunctionT& _rfnc, Args&&... _args) {
                _rfnc(std::forward<ArgTypes>(_args)...);
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

template <class... ArgTypes, size_t FunctionDataSize, template <typename, typename> class WP>
class CallPool<void(ArgTypes...), void(ArgTypes...), FunctionDataSize, WP> {
    using FunctionT = Function<void(ArgTypes...), FunctionDataSize>;
    using WorkPoolT = WP<FunctionT, FunctionT>;
    WorkPoolT wp_;

public:
    static constexpr size_t node_capacity = WorkPoolT::node_capacity;

    CallPool() {}

    auto createSynchronizationContext()
    {
        return wp_.createSynchronizationContext();
    }

    template <typename... Args>
    CallPool(
        const WorkPoolConfiguration& _cfg,
        Args&&... _args)
        : wp_(
            _cfg,
            [](FunctionT& _rfnc, Args&&... _args) {
                _rfnc(std::forward<ArgTypes>(_args)...);
                _rfnc.reset();
            },
            [](FunctionT& _rfnc, Args&&... _args) {
                _rfnc(std::forward<ArgTypes>(_args)...);
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
                _rfnc.reset();
            },
            [](FunctionT& _rfnc, Args&&... _args) {
                _rfnc(std::forward<ArgTypes>(_args)...);
            },
            std::forward<Args>(_args)...);
    }

    template <class JT>
    void push(JT&& _jb)
    {
        wp_.push(std::forward<JT>(_jb));
    }

    template <class JT>
    bool tryPush(JT&& _jb)
    {
        return wp_.tryPush(std::forward<JT>(_jb));
    }

    template <class JT>
    void pushAll(JT&& _jb)
    {
        wp_.pushAll(std::forward<JT>(_jb));
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

} // namespace solid
