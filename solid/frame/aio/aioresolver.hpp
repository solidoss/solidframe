// solid/frame/aio/aioresolver.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <functional>
#include <string>

#include "aioerror.hpp"
#include "solid/system/pimpl.hpp"
#include "solid/system/socketaddress.hpp"

namespace solid {
namespace frame {
namespace aio {

struct ResolveBase {

    ErrorCodeT error;
    int        flags;

    ResolveBase(int _flags)
        : flags(_flags)
    {
    }
};

struct DirectResolve : ResolveBase {

    std::string host;
    std::string srvc;
    int         family;
    int         type;
    int         proto;

    DirectResolve(
        const char* _host,
        const char* _srvc,
        int         _flags,
        int         _family,
        int         _type,
        int         _proto)
        : ResolveBase(_flags)
        , host(_host)
        , srvc(_srvc)
        , family(_family)
        , type(_type)
        , proto(_proto)
    {
    }

    ResolveData doRun();
};

struct ReverseResolve : ResolveBase {

    SocketAddressInet addr;

    ReverseResolve(
        const SocketAddressStub& _rsa,
        int                      _flags)
        : ResolveBase(_flags)
        , addr(_rsa)
    {
    }

    void doRun(std::string& _rhost, std::string& _rsrvc);
};

template <class Cbk>
struct DirectResolveCbk : DirectResolve {

    Cbk cbk;

    DirectResolveCbk(
        Cbk         _cbk,
        const char* _host,
        const char* _srvc,
        int         _flags,
        int         _family,
        int         _type,
        int         _proto)
        : DirectResolve(_host, _srvc, _flags, _family, _type, _proto)
        , cbk(_cbk)
    {
    }

    void operator()()
    {
        ResolveData rd;

        rd = this->doRun();
        cbk(rd, this->error);
    }
};

template <class Cbk>
struct ReverseResolveCbk : ReverseResolve {

    Cbk cbk;

    ReverseResolveCbk(
        Cbk                      _cbk,
        const SocketAddressStub& _rsa,
        int                      _flags)
        : ReverseResolve(_rsa, _flags)
        , cbk(_cbk)
    {
    }

    void operator()()
    {
        this->doRun(cbk.hostString(), cbk.serviceString());
        cbk(this->error);
    }
};

class Resolver {
    using PushFunctionT = std::function<void(std::function<void()>&&)>;
    PushFunctionT push_fnc_;

public:
    template <class F>
    Resolver(F _f)
        : push_fnc_(_f)
    {
    }
    ~Resolver() {}

    template <class Cbk>
    void requestResolve(
        Cbk         _cbk,
        const char* _host,
        const char* _srvc,
        int         _flags  = 0,
        int         _family = -1,
        int         _type   = -1,
        int         _proto  = -1)
    {
        push_fnc_(DirectResolveCbk<Cbk>(_cbk, _host, _srvc, _flags, _family, _type, _proto));
    }

    template <class Cbk>
    void requestResolve(
        Cbk                      _cbk,
        const SocketAddressStub& _rsa,
        int                      _flags = 0)
    {
        push_fnc_(ReverseResolveCbk<Cbk>(_rsa, _flags));
    }
};

} // namespace aio
} // namespace frame
} // namespace solid
