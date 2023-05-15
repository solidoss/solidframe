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

#include "aioerror.hpp"
#include "aioreactorcontext.hpp"
#include "solid/frame/aio/aiocompletion.hpp"
#include "solid/frame/aio/aiosocketbase.hpp"

namespace solid {
struct EventBase;
namespace frame {
namespace aio {

struct ActorProxy;
struct ReactorContext;

class Listener : public CompletionHandler {
    static void on_completion(CompletionHandler& _rch, ReactorContext& _rctx);
    static void on_init_completion(CompletionHandler& _rch, ReactorContext& _rctx);
    static void on_posted_accept(ReactorContext& _rctx, EventBase&&);
    static void on_dummy(ReactorContext&, SocketDevice&);

public:
    Listener(
        ActorProxy const& _ract,
        SocketDevice&&    _rsd)
        : CompletionHandler(_ract, Listener::on_init_completion)
        , s(std::move(_rsd))
    {
    }

    ~Listener()
    {
        // MUST call here and not in the ~CompletionHandler
        this->deactivate();
    }

    SocketDevice reset(ReactorContext& _rctx, SocketDevice&& _rnewdev = std::move(dummy_socket_device()));

    // const SocketDevice& device() const;

    // Returns false when the operation is scheduled for completion. On completion _f(...) will be called.
    // Returns true when operation could not be scheduled for completion - e.g. operation already in progress.
    template <typename F>
    bool postAccept(ReactorContext& _rctx, F&& _f)
    {
        if (solid_function_empty(f)) {
            f = std::forward<F>(_f);
            doPostAccept(_rctx);
            return false;
        } else {
            error(_rctx, error_already);
            return true;
        }
    }

    // Returns true when the operation completed. Check _rctx.error() for success or fail
    // Returns false when operation is scheduled for completion. On completion _f(...) will be called.
    template <typename F>
    bool accept(ReactorContext& _rctx, F&& _f, SocketDevice& _rsd)
    {
        if (solid_function_empty(f)) {
            contextBind(_rctx);

            if (this->doTryAccept(_rctx, _rsd)) {
                return true;
            }
            f = std::forward<F>(_f);
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
    typedef solid_function_t(void(ReactorContext&, SocketDevice&)) FunctionT;
    FunctionT  f;
    SocketBase s;
};

} // namespace aio
} // namespace frame
} // namespace solid
