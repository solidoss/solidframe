// solid/frame/aio/openssl/aiosecuresocket.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/utility/common.hpp"
#ifdef SOLID_ON_WINDOWS

#ifndef NOMINMAX
#define NOMINMAX
#endif

#endif

#include "openssl/ssl.h"
#include "solid/frame/aio/aiosocketbase.hpp"
#include "solid/system/socketdevice.hpp"
#include "solid/utility/function.hpp"
#include <cerrno>
#include <cstddef>
#include <expected>
#include <span>
#include <string_view>
#include <system_error>

namespace solid::frame::aio {

class ReactorContext;

namespace openssl {

class Context;

enum VerifyMode : std::uint8_t {
    VerifyModeNone             = 1,
    VerifyModePeer             = 2,
    VerifyModeFailIfNoPeerCert = 4,
    VerifyModeClientOnce       = 8,
};

using VerifyMaskT = unsigned long;

class Socket;

struct VerifyContext {
    using NativeContextT = X509_STORE_CTX;

    [[nodiscard]] NativeContextT* nativeHandle() const
    {
        return ssl_ctx;
    }

    VerifyContext(const VerifyContext&) = delete;
    VerifyContext(VerifyContext&&)      = delete;

    VerifyContext& operator=(VerifyContext&&) = delete;

private:
    friend class Socket;
    VerifyContext(NativeContextT* _ssl_ctx)
        : ssl_ctx(_ssl_ctx)
    {
    }

    NativeContextT* ssl_ctx;
};

//! A TLS socket. Operations that need to wait for the socket to become ready fail with an error
//! for which SocketBase::can_retry(error) returns true; a peer shutdown during a handshake fails
//! with an error equivalent to the error_stream_shutdown condition.
class Socket : public SocketBase {
public:
    using NativeHandleT  = SSL*;
    using VerifyMaskT    = openssl::VerifyMaskT;
    using VerifyContextT = openssl::VerifyContext;

    Socket(const Context& _rctx, SocketDevice&& _rsd);

    Socket(const Context& _rctx);

    ~Socket();

    SocketDevice reset(ReactorContext& _rctx, SocketDevice&& _rsd);

    [[nodiscard]] std::expected<void, std::error_code> create(ReactorContext& _rctx, SocketAddressStub const& _rsas);

    [[nodiscard]] std::expected<void, std::error_code> renegotiate();

    ReactorEventE filterReactorEvents(
        const ReactorEventE _evt) const;

    [[nodiscard]] std::expected<std::size_t, std::error_code> recv(ReactorContext& _rctx, std::span<std::byte> _buf);

    [[nodiscard]] std::expected<std::size_t, std::error_code> send(ReactorContext& _rctx, std::span<const std::byte> _buf);

    NativeHandleT nativeHandle() const;

    [[nodiscard]] std::expected<std::size_t, std::error_code> recvFrom(ReactorContext& _rctx, std::span<std::byte> _buf, SocketAddress& _raddr);

    [[nodiscard]] std::expected<std::size_t, std::error_code> sendTo(ReactorContext& _rctx, std::span<const std::byte> _buf, SocketAddressStub const& _rsas);

    [[nodiscard]] std::expected<void, std::error_code> secureAccept(ReactorContext& _rctx);
    [[nodiscard]] std::expected<void, std::error_code> secureConnect(ReactorContext& _rctx);
    [[nodiscard]] std::expected<void, std::error_code> secureShutdown(ReactorContext& _rctx);

    template <typename Cbk>
    [[nodiscard]] std::expected<void, std::error_code> setVerifyCallback(VerifyMaskT _verify_mask, Cbk _cbk)
    {
        verify_cbk = std::move(_cbk);
        return doPrepareVerifyCallback(_verify_mask);
    }

    [[nodiscard]] std::expected<void, std::error_code> setVerifyDepth(const int _depth);

    [[nodiscard]] std::expected<void, std::error_code> setVerifyMode(VerifyMaskT _verify_mask);

    [[nodiscard]] std::expected<void, std::error_code> setCheckHostName(std::string_view _hostname);
    [[nodiscard]] std::expected<void, std::error_code> setCheckEmail(std::string_view _email);
    [[nodiscard]] std::expected<void, std::error_code> setCheckIP(std::string_view _ip);

private:
    static int thisSSLDataIndex();
    static int contextPointerSSLDataIndex();

    void storeThisPointer();
    void clearThisPointer();

    void storeContextPointer(void* _pctx);
    void clearContextPointer();

    std::expected<void, std::error_code> doPrepareVerifyCallback(VerifyMaskT _verify_mask);

    std::expected<int, std::error_code> doMapSslResult(ReactorContext& _rctx, int _retval, const std::error_code& _err_sys, bool _on_recv, const std::error_code& _default_err);

    static int on_verify(int preverify_ok, X509_STORE_CTX* x509_ctx);

private:
    using VerifyFunctionT = Function<bool(void*, bool, VerifyContextT&)>;

    SSL*            pssl;
    bool            want_read_on_recv;
    bool            want_read_on_send;
    bool            want_write_on_recv;
    bool            want_write_on_send;
    VerifyFunctionT verify_cbk;
};

inline Socket::NativeHandleT Socket::nativeHandle() const
{
    return pssl;
}

} // namespace openssl
} // namespace solid::frame::aio
