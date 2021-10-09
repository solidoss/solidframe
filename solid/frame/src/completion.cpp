// solid/frame/src/completion.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/frame/completion.hpp"
#include "solid/frame/actor.hpp"
#include "solid/frame/reactor.hpp"
#include "solid/frame/reactorcontext.hpp"
#include "solid/system/cassert.hpp"

#include "solid/system/exception.hpp"

namespace solid {
namespace frame {
//---------------------------------------------------------------------
//----  Completion  ----
//---------------------------------------------------------------------

CompletionHandler::CompletionHandler(
    ActorProxy const& _rop,
    CallbackT         _pcall /* = &on_init_completion*/
    )
    : pprev(nullptr)
    , idxreactor(InvalidIndex())
    , call(_pcall)
{
    if (_rop.actor().registerCompletionHandler(*this)) {
        this->activate(_rop.actor());
    }
}

CompletionHandler::CompletionHandler(
    CallbackT _pcall /* = &on_init_completion*/
    )
    : pprev(nullptr)
    , idxreactor(InvalidIndex())
    , call(_pcall)
{
}

CompletionHandler::~CompletionHandler()
{
    unregister();
    //Cannot call deactivate here - must be called by inheritants in their destructor.
    //deactivate();
}

ReactorEventsE CompletionHandler::reactorEvent(ReactorContext& _rctx) const
{
    return _rctx.reactorEvent();
}

/*static*/ CompletionHandler* CompletionHandler::completion_handler(ReactorContext& _rctx)
{
    return _rctx.completionHandler();
}

Reactor& CompletionHandler::reactor(ReactorContext& _rctx) const
{
    return _rctx.reactor();
}

void CompletionHandler::error(ReactorContext& _rctx, ErrorConditionT const& _err) const
{
    _rctx.error(_err);
}

void CompletionHandler::errorClear(ReactorContext& _rctx) const
{
    _rctx.clearError();
}

void CompletionHandler::systemError(ReactorContext& _rctx, ErrorCodeT const& _err) const
{
    _rctx.systemError(_err);
}

bool CompletionHandler::activate(Actor const& _ract)
{
    Reactor* preactor = nullptr;
    if (!isActive() && (preactor = Reactor::safeSpecific()) != nullptr) {
        preactor->registerCompletionHandler(*this, _ract);
    }
    return isActive();
}

void CompletionHandler::unregister()
{
    if (isRegistered()) {
        this->pprev->pnext = this->pnext;
        this->pprev = this->pnext = nullptr;
    }
}

void CompletionHandler::deactivate()
{
    Reactor* preactor = nullptr;
    if (isActive() && (preactor = Reactor::safeSpecific()) != nullptr) {
        //the actor has entered the reactor
        preactor->unregisterCompletionHandler(*this);
        idxreactor = InvalidIndex();
    }
    if (isActive()) {
        solid_throw_log(generic_logger, "FATAL: CompletionHandler deleted/deactivated outside actor's reactor!");
    }
}

/*static*/ void CompletionHandler::on_init_completion(CompletionHandler& _rch, ReactorContext& /*_rctx*/)
{
    _rch.call = nullptr;
}

/*static*/ void CompletionHandler::on_dummy_completion(CompletionHandler&, ReactorContext&)
{
}

void CompletionHandler::addTimer(ReactorContext& _rctx, NanoTime const& _rt, size_t& _storedidx)
{
    _rctx.reactor().addTimer(*this, _rt, _storedidx);
}
void CompletionHandler::remTimer(ReactorContext& _rctx, size_t const& _storedidx)
{
    _rctx.reactor().remTimer(*this, _storedidx);
}

SocketDevice& dummy_socket_device()
{
    static SocketDevice sd;
    return sd;
}

} //namespace frame
} //namespace solid
