// solid/frame/mpipc/mpipcservice.hpp
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
#include "solid/utility/function.hpp"
#include "solid/utility/typetraits.hpp"

#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcmessageflags.hpp"

#include <memory>
#include <type_traits>

namespace solid {
namespace frame {
namespace mpipc {

class Service;
class Connection;

struct MessageHeader {
    using FlagsT = MessageFlagsValueT;

    static MessageFlagsT fetch_state_flags(const MessageFlagsT& _flags)
    {
        static const MessageFlagsT state_flags{MessageFlagsE::OnPeer, MessageFlagsE::BackOnSender, MessageFlagsE::Relayed};
        return _flags & state_flags;
    }

    MessageHeader()
        : flags_(0)
    {
    }

    MessageHeader(
        const MessageHeader& _rmsgh)
        : sender_request_id_(_rmsgh.sender_request_id_)
        , recipient_request_id_(_rmsgh.recipient_request_id_)
        , flags_(fetch_state_flags(_rmsgh.flags_).toUint32())
    {
    }

    MessageHeader& operator=(MessageHeader&& _umh)
    {
        sender_request_id_    = _umh.sender_request_id_;
        recipient_request_id_ = _umh.recipient_request_id_;
        flags_                = _umh.flags_;
        url_                  = std::move(_umh.url_);
        return *this;
    }

    RequestId   sender_request_id_;
    RequestId   recipient_request_id_;
    FlagsT      flags_;
    std::string url_;

    void clear()
    {
        sender_request_id_.clear();
        recipient_request_id_.clear();
        url_.clear();
        flags_ = 0;
    }
    template <class S>
    void solidSerializeV1(S& _rs, frame::mpipc::ConnectionContext& _rctx)
    {
        if (S::IsSerializer) {
            //because a message can be sent to multiple destinations (usign DynamicPointer)
            //on serialization we cannot use/modify the values stored by ipc::Message
            //so, we'll use ones store in the context. Because the context is volatile
            //we'll store as values.

            _rs.pushCross(sender_request_id_.index, "recipient_request_index");
            _rs.pushCross(sender_request_id_.unique, "recipient_request_unique");

            _rs.pushCross(_rctx.request_id.index, "sender_request_index");
            _rs.pushCross(_rctx.request_id.unique, "sender_request_unique");
            SOLID_CHECK(_rctx.pmessage_url, "message url must not be null");
            _rs.push(*_rctx.pmessage_url, "url");
            uint64_t tmp = _rctx.message_flags.toUint64(); //not nice but safe - better solution in future versions
            _rs.pushCross(tmp, "flags");
        } else {

            _rs.pushCross(recipient_request_id_.index, "recipient_request_index");
            _rs.pushCross(recipient_request_id_.unique, "recipient_request_unique");

            _rs.pushCross(sender_request_id_.index, "sender_request_index");
            _rs.pushCross(sender_request_id_.unique, "sender_request_unique");

            _rs.push(url_, "url");
            _rs.pushCross(flags_, "flags");
        }
    }

    template <class S>
    void solidSerializeV2(S& _rs, frame::mpipc::ConnectionContext& _rctx, std::integral_constant<bool, true> _is_serializer, const char* _name)
    {
        SOLID_CHECK(_rctx.pmessage_url, "message url must not be null");
        const uint64_t tmp = _rctx.message_flags.toUint64();
        _rs.add(tmp, _rctx, "flags").add(*_rctx.pmessage_url, _rctx, "url");
        _rs.add(_rctx.request_id.index, _rctx, "sender_request_index");
        _rs.add(_rctx.request_id.unique, _rctx, "sender_request_unique");
        _rs.add(sender_request_id_.index, _rctx, "recipient_request_index");
        _rs.add(sender_request_id_.unique, _rctx, "recipient_request_unique");
    }

    template <class S>
    void solidSerializeV2(S& _rs, frame::mpipc::ConnectionContext& _rctx, std::integral_constant<bool, false> _is_deserializer, const char* _name)
    {
        _rs.add(flags_, _rctx, "flags_").add(url_, _rctx, "url");
        _rs.add(sender_request_id_.index, _rctx, "sender_request_index");
        _rs.add(sender_request_id_.unique, _rctx, "sender_request_unique");
        _rs.add(recipient_request_id_.index, _rctx, "recipient_request_index");
        _rs.add(recipient_request_id_.unique, _rctx, "recipient_request_unique");
    }

    template <class S>
    void solidSerializeV2(S& _rs, frame::mpipc::ConnectionContext& _rctx, const char* _name)
    {
        solidSerializeV2(_rs, _rctx, std::integral_constant<bool, S::is_serializer>(), _name);
    }
};

struct Message : std::enable_shared_from_this<Message> {

    using FlagsT = MessageFlagsValueT;

    static bool is_synchronous(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::Synchronous);
    }
    static bool is_asynchronous(const MessageFlagsT& _flags)
    {
        return not is_synchronous(_flags);
    }

    static bool is_waiting_response(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::WaitResponse);
    }

    static bool is_request(const MessageFlagsT& _flags)
    {
        return is_waiting_response(_flags);
    }

    static bool is_idempotent(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::Idempotent);
    }

    static bool is_started_send(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::StartedSend);
    }

    static bool is_done_send(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::DoneSend);
    }

    static bool is_canceled(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::Canceled);
    }

    static bool is_one_shot(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::OneShotSend);
    }

    static bool is_response(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::Response);
    }

    static bool is_on_sender(const MessageFlagsT& _flags)
    {
        return !is_on_peer(_flags) && !is_back_on_sender(_flags);
    }

    static bool is_on_peer(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::OnPeer);
    }

    static bool is_back_on_sender(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::BackOnSender);
    }

    static bool is_back_on_peer(const MessageFlagsT& _flags)
    {
        return is_on_peer(_flags) && is_back_on_sender(_flags);
    }

    static bool is_relayed(const MessageFlagsT& _flags)
    {
        return _flags.has(MessageFlagsE::Relayed);
    }

    static MessageFlagsT clear_state_flags(MessageFlagsT _flags)
    {
        _flags.reset(MessageFlagsE::OnPeer).reset(MessageFlagsE::BackOnSender).reset(MessageFlagsE::Relayed);
        return _flags;
    }

    static MessageFlagsT state_flags(const MessageFlagsT& _flags)
    {
        return MessageHeader::fetch_state_flags(_flags);
    }

    static MessageFlagsT update_state_flags(MessageFlagsT _flags)
    {
        if (is_on_sender(_flags)) {
            return _flags | MessageFlagsE::OnPeer;
        } else if (is_back_on_peer(_flags)) {
            return clear_state_flags(_flags);
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

    const std::string& url() const
    {
        return header_.url_;
    }

    void clearStateFlags()
    {
        header_.flags_ = clear_state_flags(header_.flags_).toUint32();
    }

    void clearHeader()
    {
        header_.clear();
    }

    MessageFlagsT flags() const
    {
        return MessageFlagsT(header_.flags_);
    }

    template <class S, class T>
    static void solidSerializeV1(S& _rs, T& _rt, const char* _name)
    {
        _rs.push(_rt, _name);
        if (S::IsDeserializer) {
            _rs.pushCall(
                [&_rt](S& /*_rs*/, solid::frame::mpipc::ConnectionContext& _rctx, uint64_t _val, solid::ErrorConditionT& _rerr) {
                    _rt.header_ = std::move(*_rctx.pmessage_header);
                },
                0,
                "call");
        }
    }

    template <class S, class T>
    static void solidSerializeV2(S& _rs, const T& _rt, frame::mpipc::ConnectionContext& _rctx, const char* _name)
    {
        _rs.add(_rt, _rctx, _name);
    }

    template <class S, class T>
    static void solidDeserializeV2(S& _rs, T& _rt, frame::mpipc::ConnectionContext& _rctx, const char* _name)
    {
        _rs.add(
            [&_rt](S& _rs, frame::mpipc::ConnectionContext& _rctx, const char* _name) {
                _rt.header_ = std::move(*_rctx.pmessage_header);
            },
            _rctx, _name);
        _rs.add(_rt, _rctx, _name);
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

using MessageCompleteFunctionT = SOLID_FUNCTION(void(
    ConnectionContext&, MessagePointerT&, MessagePointerT&, ErrorConditionT const&));

} //namespace mpipc
} //namespace frame
} //namespace solid
