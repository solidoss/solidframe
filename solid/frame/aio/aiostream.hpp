// solid/frame/aio/aiostream.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "aiocompletion.hpp"
#include "aioerror.hpp"
#include "solid/system/common.hpp"
#include "solid/system/socketdevice.hpp"
#include <cassert>
#include <cstddef>
#include <expected>
#include <span>
#include <string_view>
#include <system_error>

namespace solid::frame::aio {

struct ActorProxy;
class ReactorContext;

template <class Sock>
class Stream : public CompletionHandler {
    using ThisT         = Stream<Sock>;
    using RecvFunctionT = solid::Function128T<void(ThisT&, ReactorContext&)>;
    using SendFunctionT = solid::Function128T<void(ThisT&, ReactorContext&)>;

    static void on_init_completion(CompletionHandler& _rch, ReactorContext& _rctx)
    {
        ThisT& rthis = static_cast<ThisT&>(_rch);
        rthis.completionCallback(&on_completion);
        // rthis.contextBind(_rctx);
        rthis.s.init(_rctx);
    }

    static void on_completion(CompletionHandler& _rch, ReactorContext& _rctx)
    {
        ThisT& rthis = static_cast<ThisT&>(_rch);

        switch (rthis.s.filterReactorEvents(rthis.reactorEvent(_rctx))) {
        case ReactorEventE::None:
            break;
        case ReactorEventE::Recv:
            rthis.doRecv(_rctx);
            break;
        case ReactorEventE::Send:
            rthis.doSend(_rctx);
            break;
        case ReactorEventE::RecvSend:
            rthis.doRecv(_rctx);
            rthis.doSend(_rctx);
            break;
        case ReactorEventE::SendRecv:
            rthis.doSend(_rctx);
            rthis.doRecv(_rctx);
            break;
        case ReactorEventE::Hangup:
        case ReactorEventE::Error:
            rthis.doError(_rctx);
            break;
        case ReactorEventE::Clear:
            rthis.doClear(_rctx);
            break;
        default:
            solid_assert_log(false, generic_logger);
        }
    }

    //-------------
    static void on_posted_recv_some(ReactorContext& _rctx, EventBase const&)
    {
        ThisT& rthis = static_cast<ThisT&>(*completion_handler(_rctx));
        solid_dbg(logger, Verbose, "");
        rthis.recv_is_posted = false;
        rthis.doRecv(_rctx);
    }

    static void on_posted_send_all(ReactorContext& _rctx, EventBase const&)
    {
        ThisT& rthis = static_cast<ThisT&>(*completion_handler(_rctx));
        solid_dbg(logger, Verbose, "");
        rthis.send_is_posted = false;
        rthis.doSend(_rctx);
    }

    static void on_dummy(ThisT& _rthis, ReactorContext& _rctx)
    {
    }

    //-------------
    template <class F>
    struct RecvSomeFunctor {
        F f;

        RecvSomeFunctor(F&& _rf)
            : f{std::forward<F>(_rf)}
        {
        }

        void operator()(ThisT& _rthis, ReactorContext& _rctx)
        {
            if (_rctx.error() || _rthis.doTryRecv(_rctx)) {
                const size_t recv_sz = _rthis.recv_buf_sz;
                F            tmp{std::forward<F>(f)};
                _rthis.doClearRecv(_rctx);
                tmp(_rctx, recv_sz);
            }
        }
    };

    template <class F>
    struct SendAllFunctor {
        F f;

        SendAllFunctor(F&& _rf)
            : f{std::forward<F>(_rf)}
        {
        }

        void operator()(ThisT& _rthis, ReactorContext& _rctx)
        {
            while (_rctx.error() || _rthis.doTrySend(_rctx)) {
                if (_rthis.send_buf_sz == _rthis.send_buf.size()) {
                    F tmp{std::forward<F>(f)};
                    _rthis.doClearSend(_rctx);
                    tmp(_rctx);
                    break;
                }
            }
        }
    };

    template <class F>
    struct ConnectFunctor {
        F f;

        ConnectFunctor(F&& _rf)
            : f{std::forward<F>(_rf)}
        {
        }

        void operator()(ThisT& _rthis, ReactorContext& _rctx)
        {
            _rthis.doCheckConnect(_rctx);
            F tmp{std::forward<F>(f)};
            _rthis.doClearSend(_rctx);
            tmp(_rctx);
        }
    };

    template <class F>
    struct SecureConnectFunctor {
        F f;

        SecureConnectFunctor(F&& _rf)
            : f{std::forward<F>(_rf)}
        {
        }

        void operator()(ThisT& _rthis, ReactorContext& _rctx)
        {
            _rthis.errorClear(_rctx);

            if (!_rthis.doHandleSecureResult(_rctx, _rthis.s.secureConnect(_rctx))) {
                return;
            }
            F tmp{std::forward<F>(f)};
            _rthis.doClearSend(_rctx);
            tmp(_rctx);
        }
    };

    template <class F>
    struct SecureAcceptFunctor {
        F f;

        SecureAcceptFunctor(F&& _rf)
            : f{std::forward<F>(_rf)}
        {
        }

        void operator()(ThisT& _rthis, ReactorContext& _rctx)
        {
            _rthis.errorClear(_rctx);

            if (!_rthis.doHandleSecureResult(_rctx, _rthis.s.secureAccept(_rctx))) {
                return;
            }
            F tmp{std::forward<F>(f)};
            _rthis.doClearRecv(_rctx);
            tmp(_rctx);
        }
    };

    template <class F>
    struct SecureShutdownFunctor {
        F f;

        SecureShutdownFunctor(F&& _rf)
            : f{std::forward<F>(_rf)}
        {
        }

        void operator()(ThisT& _rthis, ReactorContext& _rctx)
        {
            _rthis.errorClear(_rctx);

            if (!_rthis.doHandleSecureResult(_rctx, _rthis.s.secureShutdown(_rctx))) {
                return;
            }
            F tmp{std::move(f)};
            _rthis.doClearSend(_rctx);
            tmp(_rctx);
        }
    };

    template <class F>
    struct SecureVerifyFunctor {
        F f;

        SecureVerifyFunctor(F&& _rf)
            : f{std::forward<F>(_rf)}
        {
        }

        bool operator()(void* _pctx, bool _preverified, typename Sock::VerifyContextT& _rverify_ctx)
        {
            return f(*static_cast<ReactorContext*>(_pctx), _preverified, _rverify_ctx);
        }
    };

public:
    explicit Stream(
        ActorProxy const& _ract, SocketDevice&& _rsd)
        : CompletionHandler(_ract, on_init_completion)
        , s(std::move(_rsd))
    {
    }

    template <class Ctx>
    Stream(
        ActorProxy const& _ract, SocketDevice&& _rsd, const Ctx& _rctx)
        : CompletionHandler(_ract, on_init_completion)
        , s(_rctx, std::move(_rsd))
    {
    }

    Stream(
        ActorProxy const& _ract)
        : CompletionHandler(_ract, on_dummy_completion)
    {
    }

    template <class Ctx>
    explicit Stream(
        ActorProxy const& _ract, const Ctx& _rctx)
        : CompletionHandler(_ract, on_dummy_completion)
        , s(_rctx)
    {
    }

    ~Stream()
    {
        // MUST call here and not in the ~CompletionHandler
        this->deactivate(false);
    }

    [[nodiscard]] bool hasPendingRecv() const
    {
        return recv_fnc.has_value();
    }

    [[nodiscard]] bool hasPendingSend() const
    {
        return send_fnc.has_value();
    }
    SocketDevice& device()
    {
        return s.device();
    }
    [[nodiscard]] SocketDevice const& device() const
    {
        return s.device();
    }

    Sock& socket()
    {
        return s;
    }

    SocketDevice reset(ReactorContext& _rctx, SocketDevice&& _rnewdev = std::move(dummy_socket_device()))
    {
        if (s.device()) {
            remDevice(_rctx, s.device());
        }

        contextBind(_rctx);

        SocketDevice sd(s.reset(_rctx, std::move(_rnewdev)));
        if (s.device()) {
            completionCallback(&on_completion);
        }
        return sd;
    }

    // Returns false when the operation is scheduled for completion. On completion _f(_rctx, _received_size) will be called.
    // Returns true when the operation could not be scheduled - e.g. a receive is already in progress.
    template <typename F>
    bool postRecvSome(ReactorContext& _rctx, std::span<std::byte> _buf, F&& _f)
    {
        if (not recv_fnc) {
            using RealF    = typename std::decay<F>::type;
            recv_fnc       = RecvSomeFunctor<RealF>{std::forward<RealF>(_f)};
            recv_buf       = _buf;
            recv_buf_sz    = 0;
            recv_is_posted = true;
            doPostRecvSome(_rctx);
            errorClear(_rctx);
            return false;
        } else {
            error(_rctx, error_already);
            return true;
        }
    }

    // Returns true when the operation completed - _sz holds the received size, check _rctx.error() for failure.
    // Returns false when the operation is scheduled for completion. On completion _f(_rctx, _received_size) will be called.
    template <typename F>
    bool recvSome(ReactorContext& _rctx, std::span<std::byte> _buf, F&& _f, size_t& _sz)
    {
        if (not recv_fnc) {
            errorClear(_rctx);
            contextBind(_rctx);

            recv_buf    = _buf;
            recv_buf_sz = 0;

            if (doTryRecv(_rctx)) {
                _sz = recv_buf_sz;
                return true;
            } else {
                using RealF = typename std::decay<F>::type;
                recv_fnc    = RecvSomeFunctor<RealF>{std::forward<RealF>(_f)};
                return false;
            }

        } else {
            error(_rctx, error_already);
        }
        return true;
    }

    void cancelRecv(ReactorContext& _rctx)
    {
        doClearRecv(_rctx);
    }

    // Returns false when the operation is scheduled for completion. On completion _f(_rctx) will be called.
    // Returns true when the operation could not be scheduled - e.g. a send is already in progress.
    template <typename F>
    bool postSendAll(ReactorContext& _rctx, std::span<const std::byte> _buf, F&& _f)
    {
        if (not send_fnc) {
            using RealF    = typename std::decay<F>::type;
            send_fnc       = SendAllFunctor<RealF>{std::forward<RealF>(_f)};
            send_buf       = _buf;
            send_buf_sz    = 0;
            send_is_posted = true;
            doPostSendAll(_rctx);
            errorClear(_rctx);
            return false;
        } else {
            error(_rctx, error_already);
            return true;
        }
    }

    // Returns true when the operation completed - check _rctx.error() for failure.
    // Returns false when the operation is scheduled for completion. On completion _f(_rctx) will be called.
    template <typename F>
    bool sendAll(ReactorContext& _rctx, std::span<const std::byte> _buf, F&& _f)
    {
        if (not send_fnc) {
            errorClear(_rctx);
            contextBind(_rctx);

            send_buf    = _buf;
            send_buf_sz = 0;

            if (doTrySend(_rctx)) {
                if (send_buf_sz == send_buf.size()) {
                    return true;
                }
            }
            using RealF = typename std::decay<F>::type;
            send_fnc    = SendAllFunctor<RealF>{std::forward<RealF>(_f)};
            return false;
        } else {
            error(_rctx, error_already);
        }
        return true;
    }

    // Returns true when the operation completed - check _rctx.error() for failure.
    // Returns false when the operation is scheduled for completion. On completion _f(_rctx) will be called.
    template <typename F>
    bool connect(ReactorContext& _rctx, SocketAddressStub const& _rsas, F&& _f)
    {
        if (not send_fnc) {
            errorClear(_rctx);
            contextBind(_rctx);

            if (const auto crv = s.create(_rctx, _rsas); crv) {
                completionCallback(&on_completion);

                if (const auto rv = s.connect(_rctx, _rsas); rv) {
                    doCheckConnect(_rctx);
                } else if (SocketDevice::can_wait_connect(rv.error())) {
                    using RealF = typename std::decay<F>::type;
                    send_fnc    = ConnectFunctor<RealF>{std::forward<RealF>(_f)};
                    return false;
                } else {
                    error(_rctx, error_stream_system);
                    systemError(_rctx, rv.error());
                }
            } else {
                error(_rctx, error_stream_system);
                systemError(_rctx, crv.error());
                solid_assert_log(crv.error(), generic_logger);
            }

        } else {
            error(_rctx, error_already);
        }
        return true;
    }

    void shutdown(ReactorContext& /*_rctx*/)
    {
        s.shutdown();
    }

    template <typename F>
    bool secureConnect(ReactorContext& _rctx, F&& _f)
    {
        if (not send_fnc) {
            errorClear(_rctx);
            contextBind(_rctx);

            if (!doHandleSecureResult(_rctx, s.secureConnect(_rctx))) {
                using RealF = typename std::decay<F>::type;
                send_fnc    = SecureConnectFunctor<RealF>{std::forward<RealF>(_f)};
                return false;
            }
        }
        return true;
    }

    template <typename F>
    bool secureAccept(ReactorContext& _rctx, F&& _f)
    {
        if (not recv_fnc) {
            errorClear(_rctx);
            contextBind(_rctx);

            if (!doHandleSecureResult(_rctx, s.secureAccept(_rctx))) {
                using RealF = typename std::decay<F>::type;
                recv_fnc    = SecureAcceptFunctor<RealF>{std::forward<RealF>(_f)};
                return false;
            }
        }
        return true;
    }

    template <typename F>
    bool secureShutdown(ReactorContext& _rctx, F&& _f)
    {
        if (not send_fnc) {
            errorClear(_rctx);
            contextBind(_rctx);

            if (!doHandleSecureResult(_rctx, s.secureShutdown(_rctx))) {
                using RealF = typename std::decay<F>::type;
                send_fnc    = SecureShutdownFunctor<RealF>{std::forward<RealF>(_f)};
                return false;
            }
        }
        return true;
    }

    void secureRenegotiate(ReactorContext& _rctx)
    {
        if (const auto rv = s.renegotiate(); !rv) {
            error(_rctx, error_stream_system);
            systemError(_rctx, rv.error());
            solid_assert_log(rv.error(), generic_logger);
        }
    }

    template <typename F>
    void secureSetVerifyCallback(ReactorContext& _rctx, typename Sock::VerifyMaskT _mode, F&& _f)
    {
        using RealF = typename std::decay<F>::type;
        SecureVerifyFunctor<RealF> fnc_wrap{std::forward<RealF>(_f)};
        if (const auto rv = s.setVerifyCallback(_mode, fnc_wrap); !rv) {
            error(_rctx, error_stream_system);
            systemError(_rctx, rv.error());
        }
    }

    void secureSetVerifyDepth(ReactorContext& _rctx, const int _depth)
    {
        if (const auto rv = s.setVerifyDepth(_depth); !rv) {
            error(_rctx, error_stream_system);
            systemError(_rctx, rv.error());
        }
    }

    void secureSetVerifyMode(ReactorContext& _rctx, typename Sock::VerifyMaskT _mode)
    {
        if (const auto rv = s.setVerifyMode(_mode); !rv) {
            error(_rctx, error_stream_system);
            systemError(_rctx, rv.error());
        }
    }

    void secureSetCheckHostName(ReactorContext& _rctx, std::string_view _val)
    {
        if (const auto rv = s.setCheckHostName(_val); !rv) {
            error(_rctx, error_stream_system);
            systemError(_rctx, rv.error());
        }
    }

private:
    void doPostRecvSome(ReactorContext& _rctx)
    {
        reactor(_rctx).post(_rctx, on_posted_recv_some, Event(), *this);
    }
    void doPostSendAll(ReactorContext& _rctx)
    {
        reactor(_rctx).post(_rctx, on_posted_send_all, Event(), *this);
    }

    void doRecv(ReactorContext& _rctx)
    {
        if (!recv_is_posted && recv_fnc) {
            solid_dbg(logger, Verbose, "");
            errorClear(_rctx);

            recv_fnc(*this, _rctx);
        }
    }

    void doSend(ReactorContext& _rctx)
    {
        if (!send_is_posted && send_fnc) {
            errorClear(_rctx);

            send_fnc(*this, _rctx);
        }
    }

    // Returns true when the receive completed (with data, peer shutdown or error) and false when it should be retried
    bool doTryRecv(ReactorContext& _rctx)
    {
        const auto rv = s.recv(_rctx, recv_buf.subspan(recv_buf_sz));

        solid_dbg(logger, Verbose, "recv (" << (recv_buf.size() - recv_buf_sz) << ") = " << (rv ? static_cast<std::ptrdiff_t>(*rv) : -1));

        if (rv) {
            if (*rv > 0) {
                recv_buf_sz += *rv;
            } else {
                error(_rctx, error_stream_shutdown);
                recv_buf    = {};
                recv_buf_sz = 0;
            }
        } else if (Sock::can_retry(rv.error())) {
            return false;
        } else {
            recv_buf    = {};
            recv_buf_sz = 0;
            error(_rctx, error_stream_system);
            systemError(_rctx, rv.error());
        }
        return true;
    }

    // Returns true when the send completed (fully or partially, peer shutdown or error) and false when it should be retried
    bool doTrySend(ReactorContext& _rctx)
    {
        const auto rv = s.send(_rctx, send_buf.subspan(send_buf_sz));

        solid_dbg(logger, Verbose, "send (" << (send_buf.size() - send_buf_sz) << ") = " << (rv ? static_cast<std::ptrdiff_t>(*rv) : -1));

        if (rv) {
            if (*rv > 0) {
                send_buf_sz += *rv;
            } else {
                error(_rctx, error_stream_shutdown);
                send_buf    = {};
                send_buf_sz = 0;
            }
        } else if (Sock::can_retry(rv.error())) {
            return false;
        } else {
            send_buf    = {};
            send_buf_sz = 0;
            error(_rctx, error_stream_system);
            systemError(_rctx, rv.error());
        }
        return true;
    }

    // Returns true when the secure operation completed (successfully or with the error set on the context)
    // and false when it should be retried once the socket becomes ready
    bool doHandleSecureResult(ReactorContext& _rctx, const std::expected<void, std::error_code>& _rv)
    {
        if (_rv) {
            return true;
        }
        if (Sock::can_retry(_rv.error())) {
            return false;
        }
        if (_rv.error() == error_stream_shutdown) {
            error(_rctx, error_stream_shutdown);
        } else {
            error(_rctx, error_stream_system);
            systemError(_rctx, _rv.error());
        }
        return true;
    }

    void doCheckConnect(ReactorContext& _rctx)
    {
        if (const auto rv = s.checkConnect(_rctx); !rv) {
            error(_rctx, error_stream_system);
            systemError(_rctx, rv.error());
        }
    }

    void doError(ReactorContext& _rctx)
    {
        error(_rctx, error_stream_socket);
        systemError(_rctx, s.device().error());

        if (send_fnc) {
            send_buf    = {};
            send_buf_sz = 0;
            send_fnc(*this, _rctx);
        }
        if (recv_fnc) {
            recv_buf    = {};
            recv_buf_sz = 0;
            recv_fnc(*this, _rctx);
        }
    }

    void doClearRecv(ReactorContext& _rctx)
    {
        recv_fnc = nullptr;
        solid_assert_log(not recv_fnc, generic_logger);
        recv_buf    = {};
        recv_buf_sz = 0;
    }

    void doClearSend(ReactorContext& _rctx)
    {
        send_fnc = nullptr;
        solid_assert_log(not send_fnc, generic_logger);
        send_buf    = {};
        send_buf_sz = 0;
    }

    void doClear(ReactorContext& _rctx)
    {
        doClearRecv(_rctx);
        doClearSend(_rctx);
        remDevice(_rctx, s.device());
        recv_fnc = &on_dummy; // we prevent new send/recv calls
        send_fnc = &on_dummy;
    }

private:
    Sock s;

    std::span<std::byte> recv_buf;
    size_t               recv_buf_sz = 0;
    RecvFunctionT        recv_fnc;
    bool                 recv_is_posted = false;

    std::span<const std::byte> send_buf;
    size_t                     send_buf_sz = 0;
    SendFunctionT              send_fnc;
    bool                       send_is_posted = false;
};

} // namespace solid::frame::aio
