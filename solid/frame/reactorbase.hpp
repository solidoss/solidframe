// solid/frame/reactorbase.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once
#include <atomic>
#include <memory>
#if !defined(__cpp_lib_atomic_wait)
#include "solid/utility/atomic_wait"
#endif
#include "solid/frame/actorbase.hpp"
#include "solid/system/statistic.hpp"
#include "solid/utility/stack.hpp"

namespace solid {
class EventBase;
namespace frame {

class Manager;
class SchedulerBase;

struct ReactorStatisticBase : solid::Statistic {

    std::atomic_uint64_t push_while_wait_lock_count_    = {0};
    std::atomic_uint64_t push_while_wait_pushing_count_ = {0};

    void pushWhileWaitLock()
    {
        ++push_while_wait_lock_count_;
    }
    void pushWhileWaitPushing()
    {
        ++push_while_wait_pushing_count_;
    }

    std::ostream& print(std::ostream& _ros) const override;

    void clear();
};

namespace impl {

enum struct LockE : uint8_t {
    Empty = 0,
    Pushing,
    Filled,
};

struct WakeStubBase {
#if defined(__cpp_lib_atomic_wait)
    std::atomic_flag pushing_ = ATOMIC_FLAG_INIT;
    std::atomic_flag popping_ = ATOMIC_FLAG_INIT;
#else
    std::atomic_bool pushing_ = {false};
    std::atomic_bool popping_ = {false};
#endif
    std::atomic_uint8_t lock_ = {to_underlying(LockE::Empty)};

    void waitWhilePush(ReactorStatisticBase& _rstats) noexcept
    {
        while (true) {
#if defined(__cpp_lib_atomic_wait)
            const bool already_pushing = pushing_.test_and_set(std::memory_order_acquire);
#else
            bool       expected        = false;
            const bool already_pushing = !pushing_.compare_exchange_strong(expected, true, std::memory_order_acquire);
#endif
            if (!already_pushing) {
                //  wait for lock to be 0.
                uint8_t value = to_underlying(LockE::Empty);

                if (!lock_.compare_exchange_weak(value, to_underlying(LockE::Pushing))) {
                    do {
                        std::atomic_wait(&lock_, value);
                        value = to_underlying(LockE::Empty);
                    } while (!lock_.compare_exchange_weak(value, to_underlying(LockE::Pushing)));
                    _rstats.pushWhileWaitLock();
                }
                return;
            } else {
#if defined(__cpp_lib_atomic_wait)
                pushing_.wait(true);
#else
                std::atomic_wait(&pushing_, true);
#endif
                _rstats.pushWhileWaitPushing();
            }
        }
    }

    void notifyWhilePush() noexcept
    {
        lock_.store(to_underlying(LockE::Filled));
#if defined(__cpp_lib_atomic_wait)
        pushing_.clear(std::memory_order_release);
        pushing_.notify_one();
#else
        pushing_.store(false, std::memory_order_release);
        std::atomic_notify_one(&pushing_);
#endif
    }

    void notifyWhilePop() noexcept
    {
        lock_.store(to_underlying(LockE::Empty));
        std::atomic_notify_one(&lock_);
    }

    bool isFilled() const noexcept
    {
        return lock_.load() == to_underlying(LockE::Filled);
    }
};

} // namespace impl

//! The base for every selector
/*!
 * The manager will call raise when an actor needs processor
 * time, e.g. because of an event.
 */
class ReactorBase : public std::enable_shared_from_this<ReactorBase>, NonCopyable {
    typedef Stack<UniqueId> UidStackT;

    SchedulerBase& rsch;
    size_t         schidx;
    size_t         crtidx;
    UidStackT      uidstk;

public:
    virtual ~ReactorBase();
    virtual bool wake(UniqueId const& _ractuid, EventBase const& _re) = 0;
    virtual bool wake(UniqueId const& _ractuid, EventBase&& _ue)      = 0;
    virtual void stop()                                               = 0;

    bool   prepareThread(const bool _success);
    void   unprepareThread();
    size_t load() const;

protected:
    typedef std::atomic<size_t> AtomicSizeT;

    ReactorBase(
        SchedulerBase& _rsch, const size_t _schidx, const size_t _crtidx = 0)
        : crtload(0)
        , rsch(_rsch)
        , schidx(_schidx)
        , crtidx(_crtidx)
    {
    }

    void           stopActor(ActorBase& _ract, Manager& _rm);
    SchedulerBase& scheduler();
    UniqueId       popUid(ActorBase& _ract);
    void           pushUid(UniqueId const& _ruid);

    AtomicSizeT crtload;

    size_t runIndex(ActorBase& _ract) const;

private:
    friend class SchedulerBase;
    size_t idInScheduler() const;
};

using ReactorBasePtrT = std::shared_ptr<ReactorBase>;

inline SchedulerBase& ReactorBase::scheduler()
{
    return rsch;
}

inline void ReactorBase::stopActor(ActorBase& _ract, Manager& _rm)
{
    _ract.stop(_rm);
}

inline size_t ReactorBase::idInScheduler() const
{
    return schidx;
}

inline size_t ReactorBase::load() const
{
    return crtload;
}

inline void ReactorBase::pushUid(UniqueId const& _ruid)
{
    uidstk.push(_ruid);
}

inline size_t ReactorBase::runIndex(ActorBase& _ract) const
{
    return static_cast<const size_t>(_ract.runId().index);
}

} // namespace frame
} // namespace solid
