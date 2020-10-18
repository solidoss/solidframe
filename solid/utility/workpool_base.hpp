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

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/queue_lockfree.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>
#include "solid/system/cassert.hpp"

namespace solid {

extern const LoggerT workpool_logger;
//-----------------------------------------------------------------------------

struct WorkPoolConfiguration {
    size_t max_worker_count_;
    size_t max_job_queue_size_;

    explicit WorkPoolConfiguration(
        const size_t _max_worker_count   = std::thread::hardware_concurrency(),
        const size_t _max_job_queue_size = std::numeric_limits<size_t>::max())
        : max_worker_count_(_max_worker_count)
        , max_job_queue_size_(_max_job_queue_size)
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
};

} //namespace impl

} //namespace solid
