// solid/frame/aio/aioreactorcontext.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
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

#include "solid/frame/service.hpp"

#include <mutex>

namespace solid {
namespace frame {

class Service;
class Manager;

namespace aio {

class Actor;

namespace impl {
class Reactor;
} // namespace impl

class CompletionHandler;

struct ReactorContext : NonCopyable {
    ~ReactorContext()
    {
    }

    const NanoTime& nanoTime() const
    {
        return rcurrent_time_;
    }

    std::chrono::steady_clock::time_point steadyTime() const
    {
        return rcurrent_time_.timePointCast<std::chrono::steady_clock::time_point>();
    }

    ErrorCodeT const& systemError() const
    {
        return system_error_;
    }

    ErrorConditionT const& error() const
    {
        return error_;
    }

    Actor&   actor() const;
    Service& service() const;
    Manager& manager() const;

    ActorIdT              actorId() const;
    Service::ActorMutexT& actorMutex() const;

    void clearError()
    {
        error_.clear();
        system_error_.clear();
    }

private:
    friend class CompletionHandler;
    friend class impl::Reactor;
    friend class Actor;
    friend class SocketBase;

    impl::Reactor& reactor()
    {
        return rreactor_;
    }

    impl::Reactor const& reactor() const
    {
        return rreactor_;
    }
    ReactorEventE reactorEvent() const
    {
        return reactor_event_;
    }
    CompletionHandler* completionHandler() const;

    void error(ErrorConditionT const& _err)
    {
        error_ = _err;
    }

    void systemError(ErrorCodeT const& _err)
    {
        system_error_ = _err;
    }

    UniqueId actorUid() const;

    ReactorContext(const ReactorContext& _rother)
        : rreactor_(_rother.rreactor_)
        , rcurrent_time_(_rother.rcurrent_time_)
        , channel_index_(_rother.channel_index_)
        , actor_index_(_rother.actor_index_)
        , reactor_event_(_rother.reactor_event_)
    {
    }

    ReactorContext(
        impl::Reactor&  _rreactor,
        const NanoTime& _rcurrent_time)
        : rreactor_(_rreactor)
        , rcurrent_time_(_rcurrent_time)
        , channel_index_(InvalidIndex())
        , actor_index_(InvalidIndex())
        , reactor_event_(ReactorEventE::None)
    {
    }

    impl::Reactor&  rreactor_;
    const NanoTime& rcurrent_time_;
    size_t          channel_index_;
    size_t          actor_index_;
    ReactorEventE   reactor_event_;
    ErrorCodeT      system_error_;
    ErrorConditionT error_;
};

} // namespace aio
} // namespace frame
} // namespace solid
