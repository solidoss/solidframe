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

#include "solid/frame/actorbase.hpp"
#include "solid/utility/stack.hpp"

namespace solid {
class EventBase;
namespace frame {

class Manager;
class SchedulerBase;

//! The base for every selector
/*!
 * The manager will call raise when an actor needs processor
 * time, e.g. because of an event.
 */
class ReactorBase : NonCopyable {
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
