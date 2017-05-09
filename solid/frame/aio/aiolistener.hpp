// solid/frame/aio/aiolistener.hpp
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
#include "solid/system/socketdevice.hpp"

#include "aiocompletion.hpp"
#include "aioerror.hpp"
#include "aioreactorcontext.hpp"

namespace solid {
struct Event;
namespace frame {
namespace aio {

struct ObjectProxy;
struct ReactorContext;

class Listener : public CompletionHandler {
    static void on_completion(CompletionHandler& _rch, ReactorContext& _rctx);
    static void on_init_completion(CompletionHandler& _rch, ReactorContext& _rctx);
    static void on_posted_accept(ReactorContext& _rctx, Event&&);
    static void on_dummy(ReactorContext&, SocketDevice&);

public:
    Listener(
        ObjectProxy const& _robj,
        SocketDevice&&     _rsd)
        : CompletionHandler(_robj, Listener::on_init_completion)
        , sd(std::move(_rsd))
        , waitreq(ReactorWaitNone)
    {
        if (sd) {
            sd.makeNonBlocking();
        }
    }

    ~Listener()
    {
        //MUST call here and not in the ~CompletionHandler
        this->deactivate();
    }

    const SocketDevice& device() const;

    //Returns false when the operation is scheduled for completion. On completion _f(...) will be called.
    //Returns true when operation could not be scheduled for completion - e.g. operation already in progress.
    template <typename F>
    bool postAccept(ReactorContext& _rctx, F _f)
    {
        if (FUNCTION_EMPTY(f)) {
            f = _f;
            doPostAccept(_rctx);
            return false;
        } else {
            error(_rctx, error_already);
            return true;
        }
    }

    //Returns true when the operation completed. Check _rctx.error() for success or fail
    //Returns false when operation is scheduled for completion. On completion _f(...) will be called.
    template <typename F>
    bool accept(ReactorContext& _rctx, F _f, SocketDevice& _rsd)
    {
        if (FUNCTION_EMPTY(f)) {
            if (this->doTryAccept(_rctx, _rsd)) {
                return true;
            }
            f = _f;
            return false;
        } else {
            error(_rctx, error_already);
            return true;
        }
    }

private:
    void doPostAccept(ReactorContext& _rctx);
    bool doTryAccept(ReactorContext& _rctx, SocketDevice& _rsd);
    void doAccept(ReactorContext& _rctx, solid::SocketDevice& _rsd);
    void doClear(ReactorContext& _rctx);

private:
    typedef FUNCTION<void(ReactorContext&, SocketDevice&)> FunctionT;
    FunctionT            f;
    SocketDevice         sd;
    ReactorWaitRequestsE waitreq;
};

} //namespace aio
} //namespace frame
} //namespace solid
