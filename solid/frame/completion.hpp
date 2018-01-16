// solid/frame/completion.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/common.hpp"
#include "solid/frame/forwardcompletion.hpp"
#include "solid/system/cassert.hpp"
#include "solid/system/error.hpp"

namespace solid {
struct NanoTime;
namespace frame {

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

    void           completionCallback(CallbackT _pcbk);
    ReactorEventsE reactorEvent(ReactorContext& _rctx) const;
    Reactor&       reactor(ReactorContext& _rctx) const;
    void           error(ReactorContext& _rctx, ErrorConditionT const& _err) const;
    void           errorClear(ReactorContext& _rctx) const;
    void           systemError(ReactorContext& _rctx, ErrorCodeT const& _err) const;
    void           addTimer(ReactorContext& _rctx, NanoTime const& _rt, size_t& _storedidx);
    void           remTimer(ReactorContext& _rctx, size_t const& _storedidx);

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

} //namespace frame
} //namespace solid
