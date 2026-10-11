// solid/frame/aio/aiodatagram.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once
#include "solid/frame/aio/aiocompletion.hpp"
#include "solid/frame/aio/aioerror.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/system/common.hpp"
#include "solid/system/socketdevice.hpp"
#include "solid/utility/event.hpp"
#include "solid/utility/function.hpp"
#include <cstddef>
#include <expected>
#include <span>
#include <system_error>

namespace solid::frame::aio {

struct ActorProxy;
struct ReactorContex;

template <class Sock>
class Datagram : public CompletionHandler {
    using ThisT         = Datagram<Sock>;
    using RecvFunctionT = SmallFunction64T<void(ThisT&, ReactorContext&)>;
    using SendFunctionT = SmallFunction64T<void(ThisT&, ReactorContext&)>;

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

    //-----------
    static void on_posted_recv(ReactorContext& _rctx, EventBase const&)
    {
        ThisT& rthis         = static_cast<ThisT&>(*completion_handler(_rctx));
        rthis.recv_is_posted = false;
        rthis.doRecv(_rctx);
    }

    static void on_posted_send(ReactorContext& _rctx, EventBase const&)
    {
        ThisT& rthis         = static_cast<ThisT&>(*completion_handler(_rctx));
        rthis.send_is_posted = false;
        rthis.doSend(_rctx);
    }
    static void on_dummy(ThisT& _rthis, ReactorContext& _rctx)
    {
    }
    //-----------
    // Returns true when the failed operation must be reported, false when it should be retried
    static bool handle_error(ThisT& _rthis, ReactorContext& _rctx, const std::error_code& _err)
    {
        if (Sock::can_retry(_err)) {
            return false;
        }
        _rthis.error(_rctx, error_datagram_system);
        _rthis.systemError(_rctx, _err);
        solid_assert_log(_err, generic_logger);
        return true;
    }

    template <class F>
    struct RecvFunctor {
        F f;

        RecvFunctor(F&& _rf)
            : f{std::forward<F>(_rf)}
        {
        }

        void operator()(ThisT& _rthis, ReactorContext& _rctx)
        {
            size_t recv_sz = 0;

            if (!_rctx.error()) {
                const auto rv = _rthis.s.recv(_rctx, _rthis.recv_buf);

                if (rv) {
                    if (*rv > 0) {
                        recv_sz = *rv;
                    } else {
                        _rthis.error(_rctx, error_datagram_shutdown);
                    }
                } else if (!handle_error(_rthis, _rctx, rv.error())) {
                    return;
                }
            }
            F tmp{std::move(f)};
            _rthis.doClearRecv(_rctx);
            tmp(_rctx, recv_sz);
        }
    };

    template <class F>
    struct RecvFromFunctor {
        F f;

        RecvFromFunctor(F&& _rf)
            : f{std::forward<F>(_rf)}
        {
        }

        void operator()(ThisT& _rthis, ReactorContext& _rctx)
        {
            size_t        recv_sz = 0;
            SocketAddress addr;

            if (!_rctx.error()) {
                const auto rv = _rthis.s.recvFrom(_rctx, _rthis.recv_buf, addr);

                if (rv) {
                    if (*rv > 0) {
                        recv_sz = *rv;
                    } else {
                        _rthis.error(_rctx, error_datagram_shutdown);
                    }
                } else if (!handle_error(_rthis, _rctx, rv.error())) {
                    return;
                }
            }

            F tmp{std::move(f)};
            _rthis.doClearRecv(_rctx);
            tmp(_rctx, addr, recv_sz);
        }
    };

    template <class F>
    struct SendFunctor {
        F f;

        SendFunctor(F&& _rf)
            : f{std::forward<F>(_rf)}
        {
        }

        void operator()(ThisT& _rthis, ReactorContext& _rctx)
        {
            if (!_rctx.error()) {
                const auto rv = _rthis.s.send(_rctx, _rthis.send_buf);

                if (rv) {
                    if (*rv != _rthis.send_buf.size()) {
                        _rthis.error(_rctx, error_datagram_shutdown);
                    }
                } else if (!handle_error(_rthis, _rctx, rv.error())) {
                    return;
                }
            }

            F tmp{std::move(f)};
            _rthis.doClearSend(_rctx);
            tmp(_rctx);
        }
    };

    template <class F>
    struct SendToFunctor {
        F f;

        SendToFunctor(F&& _rf)
            : f{std::forward<F>(_rf)}
        {
        }

        void operator()(ThisT& _rthis, ReactorContext& _rctx)
        {
            if (!_rctx.error()) {
                const auto rv = _rthis.s.sendTo(_rctx, _rthis.send_buf, _rthis.send_addr);

                if (rv) {
                    if (*rv != _rthis.send_buf.size()) {
                        _rthis.error(_rctx, error_datagram_shutdown);
                    }
                } else if (!handle_error(_rthis, _rctx, rv.error())) {
                    return;
                }
            }

            F tmp{std::move(f)};
            _rthis.doClearSend(_rctx);
            tmp(_rctx);
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
            F tmp{std::move(f)};
            _rthis.doClearSend(_rctx);
            tmp(_rctx);
        }
    };

public:
    Datagram(
        ActorProxy const& _ract, SocketDevice&& _rsd)
        : CompletionHandler(_ract, on_init_completion)
        , s(std::move(_rsd))
    {
    }

    Datagram(
        ActorProxy const& _ract)
        : CompletionHandler(_ract, on_dummy_completion)
    {
    }

    ~Datagram()
    {
        // MUST call here and not in the ~CompletionHandler
        this->deactivate(false);
    }

    [[nodiscard]] bool hasPendingRecv() const
    {
        return recv_fnc;
    }

    [[nodiscard]] bool hasPendingSend() const
    {
        return send_fnc;
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

                } else if (SocketDevice::can_wait_connect(rv.error())) {
                    using RealF = typename std::decay<F>::type;
                    send_fnc    = ConnectFunctor<RealF>{std::forward<RealF>(_f)};
                    return false;
                } else {
                    systemError(_rctx, rv.error());
                    error(_rctx, error_datagram_system);
                    solid_assert_log(rv.error(), generic_logger);
                }
            } else {
                error(_rctx, error_datagram_system);
                systemError(_rctx, crv.error());
                solid_assert_log(crv.error(), generic_logger);
            }

        } else {
            error(_rctx, error_already);
        }
        return true;
    }

    // Returns false when the operation is scheduled for completion. On completion _f(_rctx, _address, _received_size) will be called.
    template <typename F>
    bool postRecvFrom(
        ReactorContext&      _rctx,
        std::span<std::byte> _buf,
        F&&                  _f)
    {
        if (not recv_fnc) {
            using RealF    = typename std::decay<F>::type;
            recv_fnc       = RecvFromFunctor<RealF>{std::forward<RealF>(_f)};
            recv_buf       = _buf;
            recv_is_posted = true;
            doPostRecvSome(_rctx);
            errorClear(_rctx);
            return false;
        }
        error(_rctx, error_already);
        return true;
    }

    // Returns false when the operation is scheduled for completion. On completion _f(_rctx, _received_size) will be called.
    template <typename F>
    bool postRecv(
        ReactorContext&      _rctx,
        std::span<std::byte> _buf,
        F&&                  _f)
    {
        if (not recv_fnc) {
            using RealF    = typename std::decay<F>::type;
            recv_fnc       = RecvFunctor<RealF>{std::forward<RealF>(_f)};
            recv_buf       = _buf;
            recv_is_posted = true;
            doPostRecvSome(_rctx);
            errorClear(_rctx);
            return false;
        }
        error(_rctx, error_already);
        return true;
    }

    // Returns true when the operation completed - _raddr and _sz are set, check _rctx.error() for failure.
    // Returns false when the operation is scheduled for completion. On completion _f(_rctx, _address, _received_size) will be called.
    template <typename F>
    bool recvFrom(
        ReactorContext&      _rctx,
        std::span<std::byte> _buf,
        F&&                  _f,
        SocketAddress&       _raddr,
        size_t&              _sz)
    {
        if (not recv_fnc) {
            contextBind(_rctx);

            const auto rv = s.recvFrom(_rctx, _buf, _raddr);

            _sz = 0;

            if (rv) {
                if (*rv > 0) {
                    _sz = *rv;
                    errorClear(_rctx);
                } else {
                    error(_rctx, error_datagram_shutdown);
                }
            } else if (Sock::can_retry(rv.error())) {
                recv_buf    = _buf;
                using RealF = typename std::decay<F>::type;
                recv_fnc    = RecvFromFunctor<RealF>{std::forward<RealF>(_f)};
                errorClear(_rctx);
                return false;
            } else {
                error(_rctx, error_datagram_system);
                systemError(_rctx, rv.error());
                solid_assert_log(rv.error(), generic_logger);
            }
        } else {
            error(_rctx, error_already);
        }
        return true;
    }

    // Returns true when the operation completed - _sz is set, check _rctx.error() for failure.
    // Returns false when the operation is scheduled for completion. On completion _f(_rctx, _received_size) will be called.
    template <typename F>
    bool recv(
        ReactorContext&      _rctx,
        std::span<std::byte> _buf,
        F&&                  _f,
        size_t&              _sz)
    {
        if (not recv_fnc) {
            contextBind(_rctx);

            const auto rv = s.recv(_rctx, _buf);

            _sz = 0;

            if (rv) {
                if (*rv > 0) {
                    _sz = *rv;
                    errorClear(_rctx);
                } else {
                    error(_rctx, error_datagram_shutdown);
                }
            } else if (Sock::can_retry(rv.error())) {
                recv_buf    = _buf;
                using RealF = typename std::decay<F>::type;
                recv_fnc    = RecvFunctor<RealF>{std::forward<RealF>(_f)};
                errorClear(_rctx);
                return false;
            } else {
                error(_rctx, error_datagram_system);
                systemError(_rctx, rv.error());
                solid_assert_log(rv.error(), generic_logger);
            }
        } else {
            error(_rctx, error_already);
        }
        return true;
    }

    // Returns false when the operation is scheduled for completion. On completion _f(_rctx) will be called.
    template <typename F>
    bool postSendTo(
        ReactorContext&            _rctx,
        std::span<const std::byte> _buf,
        SocketAddressStub const&   _addrstub,
        F&&                        _f)
    {
        if (not send_fnc) {
            using RealF    = typename std::decay<F>::type;
            send_fnc       = SendToFunctor<RealF>{std::forward<RealF>(_f)};
            send_buf       = _buf;
            send_addr      = _addrstub;
            send_is_posted = true;
            doPostSendAll(_rctx);
            errorClear(_rctx);
            return false;
        }
        error(_rctx, error_already);
        solid_assert_log(false, generic_logger);
        return true;
    }

    // Returns false when the operation is scheduled for completion. On completion _f(_rctx) will be called.
    template <typename F>
    bool postSend(
        ReactorContext&            _rctx,
        std::span<const std::byte> _buf,
        F&&                        _f)
    {
        if (not send_fnc) {
            using RealF    = typename std::decay<F>::type;
            send_fnc       = SendFunctor<RealF>{std::forward<RealF>(_f)};
            send_buf       = _buf;
            send_is_posted = true;
            doPostSendAll(_rctx);
            errorClear(_rctx);
            return false;
        }
        error(_rctx, error_already);
        solid_assert_log(false, generic_logger);
        return true;
    }

    // Returns true when the operation completed - check _rctx.error() for failure.
    // Returns false when the operation is scheduled for completion. On completion _f(_rctx) will be called.
    template <typename F>
    bool sendTo(
        ReactorContext&            _rctx,
        std::span<const std::byte> _buf,
        SocketAddressStub const&   _addrstub,
        F&&                        _f)
    {
        if (not send_fnc) {
            contextBind(_rctx);

            const auto rv = s.sendTo(_rctx, _buf, _addrstub);

            if (rv) {
                if (*rv == _buf.size()) {
                    errorClear(_rctx);
                } else {
                    error(_rctx, error_datagram_shutdown);
                }
            } else if (Sock::can_retry(rv.error())) {
                send_buf    = _buf;
                send_addr   = _addrstub;
                using RealF = typename std::decay<F>::type;
                send_fnc    = SendToFunctor<RealF>{std::forward<RealF>(_f)};
                errorClear(_rctx);
                return false;
            } else {
                error(_rctx, error_datagram_system);
                systemError(_rctx, rv.error());
                solid_assert_log(rv.error(), generic_logger);
            }
        } else {
            error(_rctx, error_already);
        }
        return true;
    }

    // Returns true when the operation completed - _sz is set, check _rctx.error() for failure.
    // Returns false when the operation is scheduled for completion. On completion _f(_rctx) will be called.
    template <typename F>
    bool send(
        ReactorContext&            _rctx,
        std::span<const std::byte> _buf,
        F&&                        _f,
        size_t&                    _sz)
    {
        if (not send_fnc) {
            contextBind(_rctx);

            const auto rv = s.send(_rctx, _buf);

            _sz = 0;

            if (rv) {
                if (*rv == _buf.size()) {
                    _sz = *rv;
                    errorClear(_rctx);
                } else {
                    error(_rctx, error_datagram_shutdown);
                }
            } else if (Sock::can_retry(rv.error())) {
                send_buf    = _buf;
                using RealF = typename std::decay<F>::type;
                send_fnc    = SendFunctor<RealF>{std::forward<RealF>(_f)};
                errorClear(_rctx);
                return false;
            } else {
                error(_rctx, error_datagram_system);
                systemError(_rctx, rv.error());
                solid_assert_log(rv.error(), generic_logger);
            }
        } else {
            error(_rctx, error_already);
        }
        return true;
    }

private:
    void doPostRecvSome(ReactorContext& _rctx)
    {
        reactor(_rctx).post(_rctx, on_posted_recv, Event<>(), *this);
    }
    void doPostSendAll(ReactorContext& _rctx)
    {
        reactor(_rctx).post(_rctx, on_posted_send, Event<>(), *this);
    }

    void doRecv(ReactorContext& _rctx)
    {
        if (!recv_is_posted && recv_fnc) {
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

    void doError(ReactorContext& _rctx)
    {
        error(_rctx, error_datagram_socket);
        systemError(_rctx, s.device().error());

        if (send_fnc) {
            send_fnc(*this, _rctx);
        }
        if (recv_fnc) {
            recv_fnc(*this, _rctx);
        }
    }

    void doClearRecv(ReactorContext& _rctx)
    {
        recv_fnc = nullptr;
        recv_buf = {};
    }

    void doClearSend(ReactorContext& _rctx)
    {
        send_fnc = nullptr;
        send_buf = {};
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
    Sock                       s;
    std::span<std::byte>       recv_buf;
    RecvFunctionT              recv_fnc;
    bool                       recv_is_posted = false;
    std::span<const std::byte> send_buf;
    SendFunctionT              send_fnc;
    SocketAddress              send_addr;
    bool                       send_is_posted = false;
};

} // namespace solid::frame::aio
