// solid/frame/aio/aiocompletion.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/aio/aiocommon.hpp"
#include "solid/frame/aio/aioforwardcompletion.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/system/cassert.hpp"
#include "solid/system/error.hpp"

namespace solid {
class Device;
class SocketDevice;
struct NanoTime;
namespace frame {
namespace aio {

class Object;
class Reactor;
struct ObjectProxy;
struct ReactorContext;
struct ReactorEvent;

class CompletionHandler : public ForwardCompletionHandler {
    static void on_init_completion(CompletionHandler&, ReactorContext&);

protected:
    static void               on_dummy_completion(CompletionHandler&, ReactorContext&);
    static CompletionHandler* completion_handler(ReactorContext&);
    typedef void (*CallbackT)(CompletionHandler&, ReactorContext&);

public:
    CompletionHandler(
        ObjectProxy const& _rop,
        CallbackT          _pcall = &on_init_completion);

    ~CompletionHandler();

    bool isActive() const
    {
        return idxreactor != InvalidIndex();
    }
    bool isRegistered() const
    {
        return pprev != nullptr;
    }
    bool activate(Object const& _robj);
    void deactivate();
    void unregister();

protected:
    CompletionHandler(CallbackT _pcall = &on_init_completion);

    void           completionCallback(CallbackT _pcbk = &on_dummy_completion);
    ReactorEventsE reactorEvent(ReactorContext& _rctx) const;
    Reactor&       reactor(ReactorContext& _rctx) const;
    void           error(ReactorContext& _rctx, ErrorConditionT const& _err) const;
    void           errorClear(ReactorContext& _rctx) const;
    void           systemError(ReactorContext& _rctx, ErrorCodeT const& _err) const;
    void           contextBind(ReactorContext& _rctx)const;
    void           contextUnbind(ReactorContext& _rctx)const;
    void           remDevice(ReactorContext& _rctx, Device const& _rsd);
    void           addTimer(ReactorContext& _rctx, NanoTime const& _rt, size_t& _storedidx);
    void           remTimer(ReactorContext& _rctx, size_t const& _storedidx);
    size_t         indexWithinReactor() const;

private:
    friend class Reactor;

    void handleCompletion(ReactorContext& _rctx)
    {
        (*call)(*this, _rctx);
    }

private:
    friend class Object;

private:
    ForwardCompletionHandler* pprev;
    size_t                    idxreactor; //index within reactor
    CallbackT                 call;
};

inline void CompletionHandler::completionCallback(CallbackT _pcbk)
{
    call = _pcbk;
}

inline size_t CompletionHandler::indexWithinReactor() const
{
    return idxreactor;
}

inline ReactorEventsE CompletionHandler::reactorEvent(ReactorContext& _rctx) const
{
    return _rctx.reactorEvent();
}

inline /*static*/ CompletionHandler* CompletionHandler::completion_handler(ReactorContext& _rctx)
{
    return _rctx.completionHandler();
}

inline Reactor& CompletionHandler::reactor(ReactorContext& _rctx) const
{
    return _rctx.reactor();
}

inline void CompletionHandler::error(ReactorContext& _rctx, ErrorConditionT const& _err) const
{
    _rctx.error(_err);
}

inline void CompletionHandler::errorClear(ReactorContext& _rctx) const
{
    _rctx.clearError();
}

inline void CompletionHandler::systemError(ReactorContext& _rctx, ErrorCodeT const& _err) const
{
    _rctx.systemError(_err);
}

inline void CompletionHandler::contextBind(ReactorContext& _rctx)const{
    SOLID_ASSERT(isActive());
    _rctx.channel_index_ = idxreactor;
}

inline void CompletionHandler::contextUnbind(ReactorContext& _rctx)const{
    _rctx.channel_index_ = InvalidIndex();
}

SocketDevice& dummy_socket_device();

} //namespace aio
} //namespace frame
} //namespace solid
