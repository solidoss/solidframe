// solid/frame/aio/aiosocketbase.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/aio/aiocommon.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/system/socketdevice.hpp"

namespace solid {
namespace frame {
namespace aio {

class SocketBase {
public:
    SocketBase(SocketDevice&& _rsd)
        : dev_(std::move(_rsd))
    {
        if (device()) {
            device().makeNonBlocking();
        }
    }

    SocketBase()
    {
    }

    SocketDevice reset(ReactorContext& _rctx, SocketDevice&& _rsd)
    {
        SocketDevice tmpsd = std::move(dev_);
        dev_               = std::move(_rsd);

        if (device()) {
            device().makeNonBlocking();
            init(_rctx);
        }
        return tmpsd;
    }

    SocketDevice resetAccept(ReactorContext& _rctx, SocketDevice&& _rsd)
    {
        SocketDevice tmpsd = std::move(dev_);
        dev_               = std::move(_rsd);

        if (device()) {
            device().makeNonBlocking();
            initAccept(_rctx);
        }
        return tmpsd;
    }

    void shutdown()
    {
        if (device()) {
            device().shutdownReadWrite();
        }
    }

    void initAccept(ReactorContext& _rctx)
    {
#if defined(SOLID_USE_EPOLL) || defined(SOLID_USE_KQUEUE)
        addReactorRequestEvents(_rctx, ReactorWaitRead);
#else
        addReactorRequestEvents(_rctx, ReactorWaitNone);
#endif
    }

    void init(ReactorContext& _rctx)
    {
#if defined(SOLID_USE_EPOLL) || defined(SOLID_USE_KQUEUE)
        addReactorRequestEvents(_rctx, ReactorWaitReadOrWrite);
#else
        addReactorRequestEvents(_rctx, ReactorWaitNone);
#endif
    }

    ErrorCodeT accept(ReactorContext& _rctx, SocketDevice& _rsd, bool& _can_retry)
    {
        ErrorCodeT err = device().accept(_rsd, _can_retry);
#if defined(SOLID_USE_WSAPOLL)
        if (err && _can_retry) {
            modifyReactorRequestEvents(_rctx, ReactorWaitRead);
        }
#endif
        return err;
    }

    bool create(ReactorContext& _rctx, SocketAddressStub const& _rsas, ErrorCodeT& _rerr)
    {
        _rerr = device().create(_rsas.family());
        if (!_rerr) {
            _rerr = device().makeNonBlocking();
#if defined(SOLID_USE_WSAPOLL)
            addReactorRequestEvents(_rctx, ReactorWaitNone);
#else
            addReactorRequestEvents(_rctx, ReactorWaitWrite);
#endif
        }
        return !_rerr;
    }

    bool connect(ReactorContext& _rctx, SocketAddressStub const& _rsas, bool& _can_retry, ErrorCodeT& _rerr)
    {
        _rerr = device().connect(_rsas, _can_retry);
#if defined(SOLID_USE_WSAPOLL)
        if (_rerr && _can_retry) {
            modifyReactorRequestEvents(_rctx, ReactorWaitWrite);
            addReactorConnect(_rctx);
        }
#endif
        return !_rerr;
    }

    ErrorCodeT checkConnect(ReactorContext& _rctx) const
    {
        ErrorCodeT err = device().error();
        if (!err) {
#if defined(SOLID_USE_EPOLL) || defined(SOLID_USE_KQUEUE)
            modifyReactorRequestEvents(_rctx, ReactorWaitReadOrWrite);
#else
            modifyReactorRequestEvents(_rctx, ReactorWaitNone);
#endif
        }
        return err;
    }

    SocketDevice const& device() const
    {
        return dev_;
    }

    SocketDevice& device()
    {
        return dev_;
    }

protected:
    void addReactorRequestEvents(ReactorContext& _rctx, const ReactorWaitRequestsE _req) const
    {
        _rctx.reactor().addDevice(_rctx, device(), _req);
    }
    void modifyReactorRequestEvents(ReactorContext& _rctx, const ReactorWaitRequestsE _req) const
    {
        _rctx.reactor().modDevice(_rctx, device(), _req);
    }
    void addReactorConnect(ReactorContext& _rctx) const
    {
        _rctx.reactor().addConnect(_rctx);
    }
    void remReactorConnect(ReactorContext& _rctx) const
    {
        _rctx.reactor().remConnect(_rctx);
    }

private:
    SocketDevice dev_;
};

} // namespace aio
} // namespace frame
} // namespace solid
