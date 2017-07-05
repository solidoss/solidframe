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
#include "solid/system/function.hpp"

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
        static const MessageFlagsT state_flags{MessageFlagsE::OnPeer, MessageFlagsE::BackOnSender};
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
        , flags_(fetch_state_flags(_rmsgh.flags_).toULong())
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

    template <class S>
    void solidSerialize(S& _rs, frame::mpipc::ConnectionContext& _rctx)
    {
        if (S::IsSerializer) {
            //because a message can be sent to multiple destinations (usign DynamicPointer)
            //on serialization we cannot use/modify the values stored by ipc::Message
            //so, we'll use ones store in the context. Because the context is volatile
            //we'll store as values.

            _rs.pushCross(sender_request_id_.index, "sender_request_index");
            _rs.pushCross(sender_request_id_.unique, "sender_request_unique");

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

    static MessageFlagsT clear_state_flags(MessageFlagsT _flags)
    {
        _flags.reset(MessageFlagsE::OnPeer).reset(MessageFlagsE::BackOnSender);
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

    const std::string& url() const
    {
        return header_.url_;
    }

    void clearStateFlags()
    {
        header_.flags_ = clear_state_flags(header_.flags_).toULong();
    }

    MessageFlagsT flags() const
    {
        return MessageFlagsT(header_.flags_);
    }

    template <class S, class T>
    static void solidSerialize(S& _rs, T& _rt, const char* _name)
    {
        //here we do only pushes so we can have access to context
        //using the above "serialize" function
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

} //namespace mpipc
} //namespace frame
} //namespace solid
