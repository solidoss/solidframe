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

#include "openssl/ssl.h"
#include "solid/frame/aio/aiosocketbase.hpp"
#include "solid/system/socketdevice.hpp"
#include "solid/utility/function.hpp"
#include <cerrno>

namespace solid {
namespace frame {
namespace aio {

struct ReactorContext;

namespace openssl {

class Context;

enum VerifyMode {
    VerifyModeNone             = 1,
    VerifyModePeer             = 2,
    VerifyModeFailIfNoPeerCert = 4,
    VerifyModeClientOnce       = 8,
};

using VerifyMaskT = unsigned long;

class Socket;

struct VerifyContext {
    using NativeContextT = X509_STORE_CTX;

    NativeContextT* nativeHandle() const
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

private:
    NativeContextT* ssl_ctx;
};

class Socket : public SocketBase {
public:
    using NativeHandleT  = SSL*;
    using VerifyMaskT    = openssl::VerifyMaskT;
    using VerifyContextT = openssl::VerifyContext;

    Socket(const Context& _rctx, SocketDevice&& _rsd);

    Socket(const Context& _rctx);

    ~Socket();

    SocketDevice reset(ReactorContext& _rctx, SocketDevice&& _rsd);

    bool create(ReactorContext& _rctx, SocketAddressStub const& _rsas, ErrorCodeT& _rerr);

    ErrorCodeT renegotiate(bool& _can_retry);

    ReactorEventE filterReactorEvents(
        const ReactorEventE _evt) const;

    ssize_t recv(ReactorContext& _rctx, char* _pb, size_t _bl, bool& _can_retry, ErrorCodeT& _rerr);

    ssize_t send(ReactorContext& _rctx, const char* _pb, size_t _bl, bool& _can_retry, ErrorCodeT& _rerr);

    NativeHandleT nativeHandle() const;

    ssize_t recvFrom(ReactorContext& _rctx, char* _pb, size_t _bl, SocketAddress& _addr, bool& _can_retry, ErrorCodeT& _rerr);

    ssize_t sendTo(ReactorContext& _rctx, const char* _pb, size_t _bl, SocketAddressStub const& _rsas, bool& _can_retry, ErrorCodeT& _rerr);

    bool secureAccept(ReactorContext& _rctx, bool& _can_retry, ErrorCodeT& _rerr);
    bool secureConnect(ReactorContext& _rctx, bool& _can_retry, ErrorCodeT& _rerr);
    bool secureShutdown(ReactorContext& _rctx, bool& _can_retry, ErrorCodeT& _rerr);

    template <typename Cbk>
    ErrorCodeT setVerifyCallback(VerifyMaskT _verify_mask, Cbk _cbk)
    {
        verify_cbk = std::move(_cbk);
        return doPrepareVerifyCallback(_verify_mask);
    }

    ErrorCodeT setVerifyDepth(const int _depth);

    ErrorCodeT setVerifyMode(VerifyMaskT _verify_mask);

    ErrorCodeT setCheckHostName(const std::string& _hostname);
    ErrorCodeT setCheckEmail(const std::string& _hostname);
    ErrorCodeT setCheckIP(const std::string& _hostname);

private:
    static int thisSSLDataIndex();
    static int contextPointerSSLDataIndex();

    void storeThisPointer();
    void clearThisPointer();

    void storeContextPointer(void* _pctx);
    void clearContextPointer();

    ErrorCodeT doPrepareVerifyCallback(VerifyMaskT _verify_mask);

    static int on_verify(int preverify_ok, X509_STORE_CTX* x509_ctx);

private:
    using VerifyFunctionT = solid_function_t(bool(void*, bool, VerifyContextT&));

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
} // namespace aio
} // namespace frame
} // namespace solid
