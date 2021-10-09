// solid/frame/aio/reactorcontext.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/common.hpp"
#include "solid/system/error.hpp"
#include "solid/system/nanotime.hpp"
#include "solid/system/socketdevice.hpp"

#include "solid/frame/aio/aiocommon.hpp"
#include <mutex>

namespace solid {
namespace frame {

class Service;
class Actor;
class Reactor;
class CompletionHandler;

struct ReactorContext {
    ~ReactorContext()
    {
    }

    const NanoTime& nanoTime() const
    {
        return rcrttm;
    }

    std::chrono::steady_clock::time_point steadyTime() const
    {
        return rcrttm.timePointCast<std::chrono::steady_clock::time_point>();
    }

    ErrorCodeT const& systemError() const
    {
        return syserr;
    }

    ErrorConditionT const& error() const
    {
        return err;
    }

    Actor&   actor() const;
    Service& service() const;
    Manager& manager() const;

    ActorIdT    actorId() const;
    std::mutex& actorMutex() const;

    void clearError()
    {
        err.clear();
        syserr.clear();
    }

private:
    friend class CompletionHandler;
    friend class Reactor;
    friend class Actor;

    Reactor& reactor()
    {
        return rreactor;
    }

    Reactor const& reactor() const
    {
        return rreactor;
    }
    ReactorEventsE reactorEvent() const
    {
        return reactevn;
    }
    CompletionHandler* completionHandler() const;

    void error(ErrorConditionT const& _err)
    {
        err = _err;
    }

    void systemError(ErrorCodeT const& _err)
    {
        syserr = _err;
    }

    UniqueId actorUid() const;

    ReactorContext(
        Reactor&        _rreactor,
        const NanoTime& _rcrttm)
        : rreactor(_rreactor)
        , rcrttm(_rcrttm)
        , chnidx(InvalidIndex())
        , actidx(InvalidIndex())
        , reactevn(ReactorEventNone)
    {
    }

    Reactor&        rreactor;
    const NanoTime& rcrttm;
    size_t          chnidx;
    size_t          actidx;
    ReactorEventsE  reactevn;
    ErrorCodeT      syserr;
    ErrorConditionT err;
};

} //namespace frame
} //namespace solid
