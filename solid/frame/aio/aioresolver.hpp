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

#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>

#include "aioerror.hpp"
#include "solid/system/pimpl.hpp"
#include "solid/system/socketaddress.hpp"

namespace solid::frame::aio {

struct ResolveBase {
    int flags;

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
        std::string_view _host,
        std::string_view _srvc,
        int              _flags,
        int              _family,
        int              _type,
        int              _proto)
        : ResolveBase(_flags)
        , host(_host)
        , srvc(_srvc)
        , family(_family)
        , type(_type)
        , proto(_proto)
    {
    }

    [[nodiscard]] std::expected<ResolveData, std::error_code> doRun();
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

    [[nodiscard]] std::expected<ReverseResolveData, std::error_code> doRun();
};

//! Calls the callback with the std::expected<ResolveData, std::error_code> result of the resolve
template <class Cbk>
struct DirectResolveCbk : DirectResolve {

    Cbk cbk;

    DirectResolveCbk(
        Cbk              _cbk,
        std::string_view _host,
        std::string_view _srvc,
        int              _flags,
        int              _family,
        int              _type,
        int              _proto)
        : DirectResolve(_host, _srvc, _flags, _family, _type, _proto)
        , cbk(std::move(_cbk))
    {
    }

    void operator()()
    {
        cbk(this->doRun());
    }
};

//! Calls the callback with the std::expected<ReverseResolveData, std::error_code> result of the reverse resolve
template <class Cbk>
struct ReverseResolveCbk : ReverseResolve {

    Cbk cbk;

    ReverseResolveCbk(
        Cbk                      _cbk,
        const SocketAddressStub& _rsa,
        int                      _flags)
        : ReverseResolve(_rsa, _flags)
        , cbk(std::move(_cbk))
    {
    }

    void operator()()
    {
        cbk(this->doRun());
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

    //! Asynchronously resolve the host and service - _cbk is called with a std::expected<ResolveData, std::error_code>
    template <class Cbk>
    void requestResolve(
        Cbk              _cbk,
        std::string_view _host,
        std::string_view _srvc,
        int              _flags  = 0,
        int              _family = -1,
        int              _type   = -1,
        int              _proto  = -1)
    {
        push_fnc_(DirectResolveCbk<Cbk>(std::move(_cbk), _host, _srvc, _flags, _family, _type, _proto));
    }

    //! Asynchronously reverse resolve the address - _cbk is called with a std::expected<ReverseResolveData, std::error_code>
    template <class Cbk>
    void requestResolve(
        Cbk                      _cbk,
        const SocketAddressStub& _rsa,
        int                      _flags = 0)
    {
        push_fnc_(ReverseResolveCbk<Cbk>(std::move(_cbk), _rsa, _flags));
    }
};

} // namespace solid::frame::aio
