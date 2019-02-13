// solid/frame/actor.hpp
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
#include "solid/frame/common.hpp"
#include "solid/frame/forwardcompletion.hpp"
#include "solid/frame/reactor.hpp"
#include "solid/frame/reactorcontext.hpp"
#include "solid/utility/event.hpp"

namespace solid {
namespace frame {

class Reactor;
struct ReactorContext;
class CompletionHandler;

class Actor;

struct ActorProxy {
    Actor& actor() const
    {
        return ract_;
    }

private:
    friend class Actor;
    ActorProxy(Actor& _ract)
        : ract_(_ract)
    {
    }
    ActorProxy(ActorProxy const& _rd)
        : ract_(_rd.ract_)
    {
    }
    ActorProxy& operator=(ActorProxy const& _rd);

private:
    Actor& ract_;
};

class Actor : public Dynamic<Actor, ActorBase>, ForwardCompletionHandler {
protected:
    friend class CompletionHandler;
    friend class Reactor;

    //! Constructor
    Actor();

    ActorProxy proxy()
    {
        return ActorProxy(*this);
    }

    bool registerCompletionHandler(CompletionHandler& _rch);

    void registerCompletionHandlers();

    bool isRunning() const;

    void postStop(ReactorContext& _rctx)
    {
        if (doPrepareStop(_rctx)) {
            _rctx.reactor().postActorStop(_rctx);
        }
    }

    template <class F>
    void postStop(ReactorContext& _rctx, F _f, Event&& _revent = Event())
    {
        if (doPrepareStop(_rctx)) {
            _rctx.reactor().postActorStop(_rctx, _f, _revent);
        }
    }

    template <class F>
    void post(ReactorContext& _rctx, F _f, Event&& _revent = Event())
    {
        _rctx.reactor().post(_rctx, _f, std::move(_revent));
    }

private:
    virtual void onEvent(ReactorContext& _rctx, Event&& _uevent);
    bool         doPrepareStop(ReactorContext& _rctx);
};

} //namespace frame
} //namespace solid
