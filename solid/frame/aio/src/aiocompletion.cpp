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
#include "solid/frame/aio/aioobject.hpp"
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
    ObjectProxy const& _rop,
    CallbackT          _pcall /* = &on_init_completion*/
    )
    : pprev(nullptr)
    , idxreactor(InvalidIndex())
    , call(_pcall)
{
    if (_rop.object().registerCompletionHandler(*this)) {
        this->activate(_rop.object());
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
    //cannot call deactivate here. it must be called by inheritants in their destructor
    //deactivate();
}

bool CompletionHandler::activate(Object const& _robj)
{
    Reactor* preactor = nullptr;
    if (!isActive() && (preactor = Reactor::safeSpecific())) {
        preactor->registerCompletionHandler(*this, _robj);
    }
    return isActive();
}

void CompletionHandler::unregister()
{
    if (isRegistered()) {
        this->pprev->pnext = this->pnext;
        if (this->pnext) {
            this->pnext->pprev = this->pprev;
        }
        this->pprev = this->pnext = nullptr;
    }
}

void CompletionHandler::deactivate()
{
    Reactor* preactor = nullptr;
    if (isActive() && (preactor = Reactor::safeSpecific())) {
        //the object has entered the reactor
        preactor->unregisterCompletionHandler(*this);
        idxreactor = InvalidIndex();
    }
    if (isActive()) {
        SOLID_THROW("FATAL: CompletionHandler deleted/deactivated outside object's reactor!");
    }
}

/*static*/ void CompletionHandler::on_init_completion(CompletionHandler& _rch, ReactorContext& _rctx)
{
    _rch.call = nullptr;
}

/*static*/ void CompletionHandler::on_dummy_completion(CompletionHandler&, ReactorContext&)
{
}

void CompletionHandler::remDevice(ReactorContext& _rctx, Device const& _rsd)
{
    if(isActive()){
        _rctx.reactor().remDevice(*this, _rsd);
    }
}

void CompletionHandler::addTimer(ReactorContext& _rctx, NanoTime const& _rt, size_t& _storedidx)
{
    SOLID_ASSERT(isActive());
    _rctx.reactor().addTimer(*this, _rt, _storedidx);
}
void CompletionHandler::remTimer(ReactorContext& _rctx, size_t const& _storedidx)
{
    SOLID_ASSERT(isActive());
    _rctx.reactor().remTimer(*this, _storedidx);
}

SocketDevice& dummy_socket_device()
{
    static SocketDevice sd;
    return sd;
}

} //namespace aio
} //namespace frame
} //namespace solid
