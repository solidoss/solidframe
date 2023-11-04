// solid/frame/mprpc/mprpcservice.hpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/common.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/cacheable.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/typetraits.hpp"

#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessageflags.hpp"
#include "solid/reflection/v1/reflection.hpp"
#include <memory>
#include <type_traits>

namespace solid {
namespace frame {
namespace mprpc {

class Service;
class Connection;

struct MessageRelayHeader {
    std::string uri_;

    MessageRelayHeader() = default;

    MessageRelayHeader(const std::string& _uri)
        : uri_(_uri)
    {
    }
    MessageRelayHeader(std::string&& _uri)
        : uri_(std::move(_uri))
    {
    }

    SOLID_REFLECT_V1(_rs, _rthis, _rctx)
    {
        if constexpr (std::decay_t<decltype(_rs)>::is_const_reflector) {
            _rs.add(_rctx.pmessage_relay_header_->uri_, _rctx, 1, "uri", [](auto& _rmeta) { _rmeta.maxSize(20); });
        } else {
            _rs.add(_rthis.uri_, _rctx, 1, "uri", [](auto& _rmeta) { _rmeta.maxSize(20); });
        }
    }

    bool empty() const noexcept
    {
        return uri_.empty();
    }

    void clear()
    {
        uri_.clear();
    }
};

std::ostream& operator<<(std::ostream& _ros, const MessageRelayHeader& _header);

struct MessageHeader {
    using FlagsT = MessageFlagsValueT;
    FlagsT             flags_{0};
    RequestId          sender_request_id_;
    RequestId          recipient_request_id_;
    MessageRelayHeader relay_;

    static MessageFlagsT fetch_state_flags(const MessageFlagsT& _flags)
    {
        static const MessageFlagsT state_flags{MessageFlagsE::OnPeer, MessageFlagsE::BackOnSender, MessageFlagsE::Relayed};
        return _flags & state_flags;
    }

    MessageHeader() = default;

    MessageHeader(
        const MessageHeader& _rmsgh)
        : flags_(fetch_state_flags(_rmsgh.flags_).toUnderlyingType())
        , sender_request_id_(_rmsgh.sender_request_id_)
        , recipient_request_id_(_rmsgh.recipient_request_id_)
    {
    }

    MessageHeader& operator=(MessageHeader&& _umh) noexcept
    {
        flags_                = _umh.flags_;
        sender_request_id_    = _umh.sender_request_id_;
        recipient_request_id_ = _umh.recipient_request_id_;
        relay_                = std::move(_umh.relay_);
        return *this;
    }

    MessageHeader& operator=(const MessageHeader& _rmh)
    {
        flags_                = fetch_state_flags(_rmh.flags_).toUnderlyingType();
        sender_request_id_    = _rmh.sender_request_id_;
        recipient_request_id_ = _rmh.recipient_request_id_;
        relay_                = _rmh.relay_;
        return *this;
    }

    void clear()
    {
        flags_ = 0;
        sender_request_id_.clear();
        recipient_request_id_.clear();
        relay_.clear();
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        if constexpr (std::decay_t<decltype(_rr)>::is_const_reflector) {
            const MessageFlagsValueT tmp = _rctx.message_flags.toUnderlyingType();
            _rr.add(tmp, _rctx, 1, "flags");
            _rr.add(_rctx.request_id.index, _rctx, 2, "sender_request_index");
            _rr.add(_rctx.request_id.unique, _rctx, 3, "sender_request_unique");
            _rr.add(_rthis.sender_request_id_.index, _rctx, 4, "recipient_request_index");
            _rr.add(_rthis.sender_request_id_.unique, _rctx, 5, "recipient_request_unique");
            if (_rctx.message_flags.has(MessageFlagsE::Relayed)) {
                _rr.add(_rthis.relay_, _rctx, 6, "relay");
            }
        } else {
            _rr.add(_rthis.flags_, _rctx, 1, "flags");
            _rr.add(_rthis.sender_request_id_.index, _rctx, 2, "sender_request_index");
            _rr.add(_rthis.sender_request_id_.unique, _rctx, 3, "sender_request_unique");
            _rr.add(_rthis.recipient_request_id_.index, _rctx, 4, "recipient_request_index");
            _rr.add(_rthis.recipient_request_id_.unique, _rctx, 5, "recipient_request_unique");
            _rr.add(
                [&_rthis](auto& _rr, auto& _rctx) {
                    const MessageFlagsT flags(_rthis.flags_);
                    if (flags.has(MessageFlagsE::Relayed)) {
                        _rr.add(_rthis.relay_, _rctx, 6, "relay");
                    }
                },
                _rctx);
        }
    }
};

struct Message : Cacheable {

    using FlagsT = MessageFlagsValueT;

    inline static bool is_synchronous(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::Synchronous);
    }
    inline static bool is_asynchronous(const MessageFlagsT& _flags)
    {
        return !is_synchronous(_flags);
    }

    inline static bool is_awaiting_response(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::AwaitResponse);
    }

    inline static bool is_request(const MessageFlagsT& _flags)
    {
        return is_awaiting_response(_flags);
    }

    inline static bool is_idempotent(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::Idempotent);
    }

    inline static bool is_started_send(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::StartedSend);
    }

    inline static bool is_done_send(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::DoneSend);
    }

    inline static bool is_canceled(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::Canceled);
    }

    inline static bool is_one_shot(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::OneShotSend);
    }

    inline static bool is_response(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::Response);
    }

    inline static bool is_response_part(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::ResponsePart);
    }

    inline static bool is_response_last(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::ResponseLast);
    }

    inline static bool is_on_sender(const MessageFlagsT& _flags)
    {
        return !is_on_peer(_flags) && !is_back_on_sender(_flags);
    }

    inline static bool is_on_peer(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::OnPeer);
    }

    inline static bool is_back_on_sender(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::BackOnSender);
    }

    inline static bool is_back_on_peer(const MessageFlagsT& _flags)
    {
        return is_on_peer(_flags) && is_back_on_sender(_flags);
    }

    inline static bool is_relayed(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::Relayed);
    }

    inline static MessageFlagsT clear_state_flags(MessageFlagsT _flags)
    {
        _flags.reset(MessageFlagsE::OnPeer).reset(MessageFlagsE::BackOnSender).reset(MessageFlagsE::Relayed);
        return _flags;
    }

    inline static MessageFlagsT state_flags(const MessageFlagsT& _flags)
    {
        return MessageHeader::fetch_state_flags(_flags);
    }

    inline static MessageFlagsT update_state_flags(MessageFlagsT _flags)
    {
        if (is_on_sender(_flags)) {
            return _flags | MessageFlagsE::OnPeer;
        } else if (is_back_on_peer(_flags)) {
            return (_flags | MessageFlagsE::BackOnSender).reset(MessageFlagsE::OnPeer);
        } else if (is_on_peer(_flags)) {
            return (_flags | MessageFlagsE::BackOnSender).reset(MessageFlagsE::OnPeer);
        } else /* if(is_back_on_sender(_flags))*/ {
            return _flags | MessageFlagsE::OnPeer;
        }
    }

    Message()
    {
    }

    Message(Message const& _rmsg)
        : header_(_rmsg.header_)
    {
    }

    virtual ~Message();

    void header(MessageHeader&& _umh)
    {
        header_ = std::move(_umh);
    }

    void header(const MessageHeader& _umh)
    {
        header_ = _umh;
    }

    void header(frame::mprpc::ConnectionContext& _rctx)
    {
        header_ = std::move(*_rctx.pmessage_header);
    }

    const MessageHeader& header() const
    {
        return header_;
    }

    Message& operator=(const Message& _other)
    {
        header(_other.header_);
        return *this;
    }

    bool isOnSender() const
    {
        return is_on_sender(flags());
    }
    bool isOnPeer() const
    {
        return is_on_peer(flags());
    }
    bool isBackOnSender() const
    {
        return is_back_on_sender(flags());
    }

    bool isBackOnPeer() const
    {
        return is_back_on_peer(flags());
    }

    bool isRelayed() const
    {
        return is_relayed(flags());
    }

    bool isResponse() const
    {
        return is_response(flags());
    }

    bool isResponsePart() const
    {
        return is_response_part(flags());
    }

    bool isResponseLast() const
    {
        return is_response_last(flags());
    }

    const std::string& uri() const
    {
        return header_.relay_.uri_;
    }

    void clearStateFlags()
    {
        header_.flags_ = clear_state_flags(header_.flags_).toUnderlyingType();
    }

    void clearHeader()
    {
        header_.clear();
    }

    MessageFlagsT flags() const
    {
        return MessageFlagsT(header_.flags_);
    }

    RequestId const& senderRequestId() const
    {
        return header_.sender_request_id_;
    }

private:
    friend class Service;
    friend class TestEntryway;
    friend class Connection;
    friend class MessageWriter;

    RequestId const& requestId() const
    {
        return header_.recipient_request_id_;
    }

private:
    MessageHeader header_;
};

using MessagePointerT = std::shared_ptr<Message>;

using MessageCompleteFunctionT = solid_function_t(void(
    ConnectionContext&, MessagePointerT&, MessagePointerT&, ErrorConditionT const&));

} // namespace mprpc
} // namespace frame
} // namespace solid
