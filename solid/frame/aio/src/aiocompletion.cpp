// solid/frame/src/aiocompletion.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/frame/aio/aiocompletion.hpp"
#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/system/cassert.hpp"

#include "solid/system/exception.hpp"

namespace solid {
namespace frame {
namespace aio {
//---------------------------------------------------------------------
//----  Completion  ----
//---------------------------------------------------------------------

CompletionHandler::CompletionHandler(
    ActorProxy const& _rap,
    CallbackT         _pcall /* = &on_init_completion*/
    )
    : pprev(nullptr)
    , idxreactor(InvalidIndex())
    , call(_pcall)
{
    if (_rap.actor().registerCompletionHandler(*this)) {
        this->activate(_rap.actor());
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
    // cannot call deactivate here. it must be called by inheritants in their destructor
    // deactivate();
}

bool CompletionHandler::activate(Actor const& _ract)
{
    impl::Reactor* preactor = nullptr;
    if (!isActive() && (preactor = impl::Reactor::safeSpecific()) != nullptr) {
        preactor->registerCompletionHandler(*this, _ract);
    }
    return isActive();
}

void CompletionHandler::unregister()
{
    if (isRegistered()) {
        this->pprev->pnext = this->pnext;
        if (this->pnext != nullptr) {
            this->pnext->pprev = this->pprev;
        }
        this->pprev = this->pnext = nullptr;
    }
}

void CompletionHandler::deactivate(const bool _check)
{
    impl::Reactor* preactor = nullptr;
    if (isActive() && (preactor = impl::Reactor::safeSpecific()) != nullptr) {
        // the actor has entered the reactor
        preactor->unregisterCompletionHandler(*this);
        idxreactor = InvalidIndex();
    }
    if (_check) {
        solid_check_log(!isActive(), generic_logger, "FATAL: CompletionHandler deleted/deactivated outside actor's reactor!");
    }
}

/*static*/ void CompletionHandler::on_init_completion(CompletionHandler& _rch, ReactorContext& /*_rctx*/)
{
    _rch.call = nullptr;
}

/*static*/ void CompletionHandler::on_dummy_completion(CompletionHandler&, ReactorContext&)
{
}

void CompletionHandler::remDevice(ReactorContext& _rctx, Device const& _rsd)
{
    if (isActive()) {
        _rctx.reactor().remDevice(*this, _rsd);
    }
}

void CompletionHandler::addTimer(ReactorContext& _rctx, NanoTime const& _rt, size_t& _storedidx)
{
    solid_assert_log(isActive(), generic_logger);
    _rctx.reactor().addTimer(*this, _rt, _storedidx);
}
void CompletionHandler::remTimer(ReactorContext& _rctx, size_t const& _storedidx)
{
    if (isActive()) {
        _rctx.reactor().remTimer(*this, _storedidx);
    }
}

SocketDevice& dummy_socket_device()
{
    static SocketDevice sd;
    return sd;
}

} // namespace aio
} // namespace frame
} // namespace solid
