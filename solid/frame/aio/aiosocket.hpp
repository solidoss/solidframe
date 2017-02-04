// solid/frame/aio/aiosocket.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_SOCKET_HPP
#define SOLID_FRAME_AIO_SOCKET_HPP

#include "solid/frame/aio/aiocommon.hpp"
#include "solid/system/socketdevice.hpp"
#include <cerrno>

namespace solid {
namespace frame {
namespace aio {

class Socket {
public:
    using VerifyMaskT = int;

    Socket(SocketDevice&& _rsd)
        : sd(std::move(_rsd))
    {
        if (sd.ok()) {
            sd.makeNonBlocking();
        }
    }

    Socket()
    {
    }

    SocketDevice reset(SocketDevice&& _rsd)
    {
        SocketDevice tmpsd = std::move(sd);
        sd                 = std::move(_rsd);
        return tmpsd;
    }

    void shutdown()
    {
        if (sd.ok()) {
            sd.shutdownReadWrite();
        }
    }

    bool create(SocketAddressStub const& _rsas, ErrorCodeT& _rerr)
    {
        _rerr = sd.create(_rsas.family());
        if (!_rerr) {
            _rerr = sd.makeNonBlocking();
        }
        return !_rerr;
    }

    bool connect(SocketAddressStub const& _rsas, bool& _can_retry, ErrorCodeT& _rerr)
    {
        _rerr = sd.connect(_rsas, _can_retry);
        return !_rerr;
    }

    ReactorEventsE filterReactorEvents(
        const ReactorEventsE _evt) const
    {
        return _evt;
    }

    int recv(void* /*_pctx*/, char* _pb, size_t _bl, bool& _can_retry, ErrorCodeT& _rerr)
    {
        return sd.recv(_pb, _bl, _can_retry, _rerr);
    }

    int send(void* /*_pctx*/, const char* _pb, size_t _bl, bool& _can_retry, ErrorCodeT& _rerr)
    {
        return sd.send(_pb, _bl, _can_retry, _rerr);
    }

    SocketDevice const& device() const
    {
        return sd;
    }

    SocketDevice& device()
    {
        return sd;
    }

    int recvFrom(char* _pb, size_t _bl, SocketAddress& _addr, bool& _can_retry, ErrorCodeT& _rerr)
    {
        return sd.recv(_pb, _bl, _addr, _can_retry, _rerr);
    }

    int sendTo(const char* _pb, size_t _bl, SocketAddressStub const& _rsas, bool& _can_retry, ErrorCodeT& _rerr)
    {
        return sd.send(_pb, _bl, _rsas, _can_retry, _rerr);
    }

private:
    SocketDevice sd;
};

} //namespace aio
} //namespace frame
} //namespace solid

#endif
