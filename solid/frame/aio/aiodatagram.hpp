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

namespace solid {
namespace frame {
namespace aio {

struct ObjectProxy;
struct ReactorContex;

template <class Sock>
class Datagram : public CompletionHandler {
    using ThisT         = Datagram<Sock>;
    using RecvFunctionT = SOLID_FUNCTION(void(ThisT&, ReactorContext&));
    using SendFunctionT = SOLID_FUNCTION(void(ThisT&, ReactorContext&));

    static void on_init_completion(CompletionHandler& _rch, ReactorContext& _rctx)
    {
        ThisT& rthis = static_cast<ThisT&>(_rch);
        rthis.completionCallback(&on_completion);
        rthis.contextBind(_rctx);
        rthis.s.init(_rctx);
    }

    static void on_completion(CompletionHandler& _rch, ReactorContext& _rctx)
    {
        ThisT& rthis = static_cast<ThisT&>(_rch);

        switch (rthis.s.filterReactorEvents(rthis.reactorEvent(_rctx))) {
        case ReactorEventNone:
            break;
        case ReactorEventRecv:
            rthis.doRecv(_rctx);
            break;
        case ReactorEventSend:
            rthis.doSend(_rctx);
            break;
        case ReactorEventRecvSend:
            rthis.doRecv(_rctx);
            rthis.doSend(_rctx);
            break;
        case ReactorEventSendRecv:
            rthis.doSend(_rctx);
            rthis.doRecv(_rctx);
            break;
        case ReactorEventHangup:
        case ReactorEventError:
            rthis.doError(_rctx);
            break;
        case ReactorEventClear:
            rthis.doClear(_rctx);
            break;
        default:
            SOLID_ASSERT(false);
        }
    }

    //-----------
    static void on_posted_recv(ReactorContext& _rctx, Event const&)
    {
        ThisT& rthis         = static_cast<ThisT&>(*completion_handler(_rctx));
        rthis.recv_is_posted = false;
        rthis.doRecv(_rctx);
    }

    static void on_posted_send(ReactorContext& _rctx, Event const&)
    {
        ThisT& rthis         = static_cast<ThisT&>(*completion_handler(_rctx));
        rthis.send_is_posted = false;
        rthis.doSend(_rctx);
    }
    static void on_dummy(ThisT& _rthis, ReactorContext& _rctx)
    {
    }
    //-----------
    template <class F>
    struct RecvFunctor {
        F f;

        RecvFunctor(F& _rf)
            : f{std::move(_rf)}
        {
        }

        void operator()(ThisT& _rthis, ReactorContext& _rctx)
        {
            size_t recv_sz = 0;

            if (!_rctx.error()) {
                bool       can_retry;
                ErrorCodeT err;
                ssize_t    rv = _rthis.s.recv(_rctx, _rthis.recv_buf, _rthis.recv_buf_cp, can_retry, err);

                if (rv > 0) {
                    recv_sz = rv;
                } else if (rv == 0) {
                    _rthis.error(_rctx, error_datagram_shutdown);
                } else if (rv == -1) {
                    if (can_retry) {
                        return;
                    } else {
                        _rthis.error(_rctx, error_datagram_system);
                        _rthis.systemError(_rctx, err);
                        SOLID_ASSERT(err);
                    }
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

        RecvFromFunctor(F& _rf)
            : f{std::move(_rf)}
        {
        }

        void operator()(ThisT& _rthis, ReactorContext& _rctx)
        {
            size_t        recv_sz = 0;
            SocketAddress addr;

            if (!_rctx.error()) {
                bool       can_retry;
                ErrorCodeT err;
                ssize_t    rv = _rthis.s.recvFrom(_rctx, _rthis.recv_buf, _rthis.recv_buf_cp, addr, can_retry, err);

                if (rv > 0) {
                    recv_sz = rv;
                } else if (rv == 0) {
                    _rthis.error(_rctx, error_datagram_shutdown);
                } else if (rv == -1) {
                    if (can_retry) {
                        return;
                    } else {
                        _rthis.error(_rctx, error_datagram_system);
                        _rthis.systemError(_rctx, err);
                        SOLID_ASSERT(err);
                    }
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

        SendFunctor(F& _rf)
            : f{std::move(_rf)}
        {
        }

        void operator()(ThisT& _rthis, ReactorContext& _rctx)
        {
            if (!_rctx.error()) {
                bool       can_retry;
                ErrorCodeT err;
                ssize_t    rv = _rthis.s.send(_rctx, _rthis.send_buf, _rthis.send_buf_cp, can_retry, err);

                if (rv == _rthis.send_buf_cp) {
                } else if (rv >= 0) {
                    _rthis.error(_rctx, error_datagram_shutdown);
                } else if (rv == -1) {
                    if (can_retry) {
                        return;
                    } else {
                        _rthis.error(_rctx, error_datagram_system);
                        _rthis.systemError(_rctx, err);
                        SOLID_ASSERT(err);
                    }
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

        SendToFunctor(F& _rf)
            : f{std::move(_rf)}
        {
        }

        void operator()(ThisT& _rthis, ReactorContext& _rctx)
        {
            if (!_rctx.error()) {
                bool       can_retry;
                ErrorCodeT err;
                ssize_t    rv = _rthis.s.sendTo(_rctx, _rthis.send_buf, _rthis.send_buf_cp, _rthis.send_addr, can_retry, err);

                if (rv == static_cast<int>(_rthis.send_buf_cp)) {
                } else if (rv >= 0) {
                    _rthis.error(_rctx, error_datagram_shutdown);
                } else if (rv == -1) {
                    if (can_retry) {
                        return;
                    } else {
                        _rthis.error(_rctx, error_datagram_system);
                        _rthis.systemError(_rctx, err);
                        SOLID_ASSERT(err);
                    }
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

        ConnectFunctor(F& _rf)
            : f{std::move(_rf)}
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
        ObjectProxy const& _robj, SocketDevice&& _rsd)
        : CompletionHandler(_robj, on_init_completion)
        , s(std::move(_rsd))
        , recv_buf(nullptr)
        , recv_buf_cp(0)
        , recv_is_posted(false)
        , send_buf(nullptr)
        , send_buf_cp(0)
        , send_is_posted(false)
    {
    }

    Datagram(
        ObjectProxy const& _robj)
        : CompletionHandler(_robj, on_dummy_completion)
        , recv_buf(nullptr)
        , recv_buf_cp(0)
        , recv_is_posted(false)
        , send_buf(nullptr)
        , send_buf_cp(0)
        , send_is_posted(false)
    {
    }

    ~Datagram()
    {
        //MUST call here and not in the ~CompletionHandler
        this->deactivate();
    }

    bool hasPendingRecv() const
    {
        return !SOLID_FUNCTION_EMPTY(recv_fnc);
    }

    bool hasPendingSend() const
    {
        return !SOLID_FUNCTION_EMPTY(send_fnc);
    }

    template <typename F>
    bool connect(ReactorContext& _rctx, SocketAddressStub const& _rsas, F _f)
    {
        if (SOLID_FUNCTION_EMPTY(send_fnc)) {
            ErrorCodeT err;

            errorClear(_rctx);
			contextBind(_rctx);

            if (s.create(_rctx, _rsas, err)) {
                completionCallback(&on_completion);

                bool can_retry;
                bool rv = s.connect(_rctx, _rsas, can_retry, err);
                if (rv) {

                } else if (can_retry) {
                    send_fnc = ConnectFunctor<F>(_f);
                    return false;
                } else {
                    systemError(_rctx, err);
                    error(_rctx, error_datagram_system);
                    SOLID_ASSERT(err);
                }
            } else {
                error(_rctx, error_datagram_system);
                systemError(_rctx, err);
                SOLID_ASSERT(err);
            }

        } else {
            error(_rctx, error_already);
        }
        return true;
    }

    template <typename F>
    bool postRecvFrom(
        ReactorContext& _rctx,
        char* _buf, size_t _bufcp,
        F _f)
    {
        if (SOLID_FUNCTION_EMPTY(recv_fnc)) {
            recv_fnc       = RecvFromFunctor<F>(_f);
            recv_buf       = _buf;
            recv_buf_cp    = _bufcp;
            recv_is_posted = true;
            doPostRecvSome(_rctx);
            errorClear(_rctx);
            return false;
        } else {
            error(_rctx, error_already);
            return true;
        }
    }

    template <typename F>
    bool postRecv(
        ReactorContext& _rctx,
        char* _buf, size_t _bufcp,
        F _f)
    {
        if (SOLID_FUNCTION_EMPTY(recv_fnc)) {
            recv_fnc       = RecvFunctor<F>(_f);
            recv_buf       = _buf;
            recv_buf_cp    = _bufcp;
            recv_is_posted = true;
            doPostRecvSome(_rctx);
            errorClear(_rctx);
            return false;
        } else {
            error(_rctx, error_already);
            return true;
        }
    }

    template <typename F>
    bool recvFrom(
        ReactorContext& _rctx,
        char* _buf, size_t _bufcp,
        F              _f,
        SocketAddress& _raddr,
        size_t&        _sz)
    {
        if (SOLID_FUNCTION_EMPTY(recv_fnc)) {
			contextBind(_rctx);

            bool       can_retry;
            ErrorCodeT err;
            ssize_t    rv = s.recvFrom(_rctx, _buf, _bufcp, _raddr, can_retry, err);

            if (rv == static_cast<ssize_t>(_bufcp)) {
                _sz = rv;
                errorClear(_rctx);
            } else if (rv >= 0) {
                error(_rctx, error_datagram_shutdown);
                _sz = 0;
            } else if (rv == -1) {
                _sz = 0;
                if (can_retry) {
                    recv_buf    = _buf;
                    recv_buf_cp = _bufcp;
                    recv_fnc    = RecvFromFunctor<F>(_f);
                    errorClear(_rctx);
                    return false;
                } else {
                    error(_rctx, error_datagram_system);
                    systemError(_rctx, err);
                    SOLID_ASSERT(err);
                }
            }
        } else {
            error(_rctx, error_already);
        }
        return true;
    }

    template <typename F>
    bool recv(
        ReactorContext& _rctx,
        char* _buf, size_t _bufcp,
        F       _f,
        size_t& _sz)
    {
        if (SOLID_FUNCTION_EMPTY(recv_fnc)) {
			contextBind(_rctx);

            bool       can_retry;
            ErrorCodeT err;
            ssize_t    rv = s.recv(_rctx, _buf, _bufcp, can_retry, err);

            if (rv == _bufcp) {
                _sz = rv;
                errorClear(_rctx);
            } else if (rv >= 0) {
                error(_rctx, error_datagram_shutdown);
                _sz = 0;
            } else if (rv == -1) {
                _sz = 0;
                if (can_retry) {
                    recv_buf    = _buf;
                    recv_buf_cp = _bufcp;
                    recv_fnc    = RecvFunctor<F>(_f);
                    errorClear(_rctx);
                    return false;
                } else {
                    error(_rctx, error_datagram_system);
                    systemError(_rctx, err);
                    SOLID_ASSERT(err);
                }
            }
        } else {
            error(_rctx, error_already);
        }
        return true;
    }

    template <typename F>
    bool postSendTo(
        ReactorContext& _rctx,
        const char* _buf, size_t _bufcp,
        SocketAddressStub const& _addrstub,
        F                        _f)
    {
        if (SOLID_FUNCTION_EMPTY(send_fnc)) {
            send_fnc       = SendToFunctor<F>(_f);
            send_buf       = _buf;
            send_buf_cp    = _bufcp;
            send_addr      = _addrstub;
            send_is_posted = true;
            doPostSendAll(_rctx);
            errorClear(_rctx);
            return false;
        } else {
            error(_rctx, error_already);
            SOLID_ASSERT(false);
            return true;
        }
    }

    template <typename F>
    bool postSend(
        ReactorContext& _rctx,
        const char* _buf, size_t _bufcp,
        F _f)
    {
        if (SOLID_FUNCTION_EMPTY(send_fnc)) {
            send_fnc       = SendFunctor<F>(_f);
            send_buf       = _buf;
            send_buf_cp    = _bufcp;
            send_is_posted = true;
            doPostSendAll(_rctx);
            errorClear(_rctx);
            return false;
        } else {
            error(_rctx, error_already);
            SOLID_ASSERT(false);
            return true;
        }
    }

    template <typename F>
    bool sendTo(
        ReactorContext& _rctx,
        const char* _buf, size_t _bufcp,
        SocketAddressStub const& _addrstub,
        F                        _f)
    {
        if (SOLID_FUNCTION_EMPTY(send_fnc)) {
			contextBind(_rctx);

            bool       can_retry;
            ErrorCodeT err;
            ssize_t    rv = s.sendTo(_rctx, _buf, _bufcp, _addrstub, can_retry, err);

            if (rv == static_cast<ssize_t>(_bufcp)) {
                errorClear(_rctx);
            } else if (rv >= 0) {
                error(_rctx, error_datagram_shutdown);
            } else if (rv == -1) {
                if (can_retry) {
                    send_buf    = _buf;
                    send_buf_cp = _bufcp;
                    send_addr   = _addrstub;
                    send_fnc    = SendToFunctor<F>(_f);
                    errorClear(_rctx);
                    return false;
                } else {
                    error(_rctx, error_datagram_system);
                    systemError(_rctx, err);
                    SOLID_ASSERT(err);
                }
            }
        } else {
            error(_rctx, error_already);
        }
        return true;
    }

    template <typename F>
    bool send(
        ReactorContext& _rctx,
        const char* _buf, size_t _bufcp,
        F       _f,
        size_t& _sz)
    {
        if (SOLID_FUNCTION_EMPTY(send_fnc)) {
			contextBind(_rctx);

            bool       can_retry;
            ErrorCodeT err;
            ssize_t    rv = s.sendTo(_rctx, _buf, _bufcp, can_retry, err);

            if (rv == _bufcp) {
                errorClear(_rctx);
            } else if (rv >= 0) {
                error(_rctx, error_datagram_shutdown);
            } else if (rv < 0) {
                if (can_retry) {
                    send_buf    = _buf;
                    send_buf_cp = _bufcp;
                    send_fnc    = SendFunctor<F>(_f);
                    errorClear(_rctx);
                    return false;
                } else {
                    error(_rctx, error_datagram_system);
                    systemError(_rctx, err);
                    SOLID_ASSERT(err);
                }
            }
        } else {
            error(_rctx, error_already);
        }
        return true;
    }

private:
    void doPostRecvSome(ReactorContext& _rctx)
    {
        reactor(_rctx).post(_rctx, on_posted_recv, Event(), *this);
    }
    void doPostSendAll(ReactorContext& _rctx)
    {
        reactor(_rctx).post(_rctx, on_posted_send, Event(), *this);
    }

    void doRecv(ReactorContext& _rctx)
    {
        if (!recv_is_posted && !SOLID_FUNCTION_EMPTY(recv_fnc)) {
            errorClear(_rctx);
			contextBind(_rctx);
            recv_fnc(*this, _rctx);
        }
    }

    void doSend(ReactorContext& _rctx)
    {
        if (!send_is_posted && !SOLID_FUNCTION_EMPTY(send_fnc)) {
            errorClear(_rctx);
			contextBind(_rctx);
            send_fnc(*this, _rctx);
        }
    }

    void doError(ReactorContext& _rctx)
    {
        error(_rctx, error_datagram_socket);
        //TODO: set proper system error based on socket error

        if (!SOLID_FUNCTION_EMPTY(send_fnc)) {
            send_fnc(*this, _rctx);
        }
        if (!SOLID_FUNCTION_EMPTY(recv_fnc)) {
            recv_fnc(*this, _rctx);
        }
    }

    void doClearRecv(ReactorContext& _rctx)
    {
        SOLID_FUNCTION_CLEAR(recv_fnc);
        recv_buf    = nullptr;
        recv_buf_cp = 0;
    }

    void doClearSend(ReactorContext& _rctx)
    {
        SOLID_FUNCTION_CLEAR(send_fnc);
        send_buf    = nullptr;
        recv_buf_cp = 0;
    }
    void doClear(ReactorContext& _rctx)
    {
        doClearRecv(_rctx);
        doClearSend(_rctx);
        remDevice(_rctx, s.device());
        recv_fnc = &on_dummy; //we prevent new send/recv calls
        send_fnc = &on_dummy;
    }

private:
    Sock          s;
    char*         recv_buf;
    size_t        recv_buf_cp;
    RecvFunctionT recv_fnc;
    bool          recv_is_posted;

    const char*   send_buf;
    size_t        send_buf_cp;
    SendFunctionT send_fnc;
    SocketAddress send_addr;
    bool          send_is_posted;
};

} //namespace aio
} //namespace frame
} //namespace solid
