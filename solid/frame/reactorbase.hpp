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
using AtomicIndexT        = std::atomic_size_t;
using AtomicIndexValueT   = std::atomic_size_t::value_type;
using AtomicCounterT      = std::atomic<uint8_t>;
using AtomicCounterValueT = AtomicCounterT::value_type;

template <class IndexT>
inline constexpr static auto computeCounter(const IndexT _index, const size_t _capacity) noexcept
{
    return (_index / _capacity) & std::numeric_limits<AtomicCounterValueT>::max();
}

struct WakeStubBase {
    AtomicCounterT produce_count_{0};
    AtomicCounterT consume_count_{static_cast<AtomicCounterValueT>(-1)};

    template <typename Statistic>
    void waitWhilePush(Statistic& _rstats, const AtomicCounterValueT _count, const size_t _spin_count = 1) noexcept
    {
        auto spin = _spin_count;
        while (true) {
            const auto cnt = produce_count_.load();
            if (cnt == _count) {
                break;
            } else if (_spin_count && !spin--) {
                _rstats.pushWhileWaitLock();
                std::atomic_wait_explicit(&produce_count_, cnt, std::memory_order_relaxed);
                spin = _spin_count;
            }
        }
    }

    void notifyWhilePush() noexcept
    {
        ++consume_count_;
        std::atomic_notify_all(&consume_count_);
    }

    void notifyWhilePop() noexcept
    {
        ++produce_count_;
        std::atomic_notify_all(&produce_count_);
    }

    bool isFilled(const uint64_t _id, const size_t _capacity) const
    {
        const auto                count          = consume_count_.load(std::memory_order_relaxed);
        const AtomicCounterValueT expected_count = computeCounter(_id, _capacity);
        return count == expected_count;
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
    std::atomic_size_t crtload = {0};

    ReactorBase(
        SchedulerBase& _rsch, const size_t _schidx, const size_t _crtidx = 0)
        : rsch(_rsch)
        , schidx(_schidx)
        , crtidx(_crtidx)
    {
    }

    void           stopActor(ActorBase& _ract, Manager& _rm);
    SchedulerBase& scheduler();
    UniqueId       popUid(ActorBase& _ract);
    void           pushUid(UniqueId const& _ruid);

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
