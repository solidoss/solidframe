// solid/frame/mpipc/mpipcservice.hpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_MPIPC_MPIPCMESSAGE_HPP
#define SOLID_FRAME_MPIPC_MPIPCMESSAGE_HPP

#include "solid/system/common.hpp"
#include "solid/system/function.hpp"

#include "solid/frame/mpipc/mpipccontext.hpp"

#include <memory>
#include <type_traits>

namespace solid {
namespace frame {
namespace mpipc {

class Service;
class Connection;

enum struct MessageFlags : MessageFlagsValueT {
    FirstFlagIndex = 0, //for rezerved flags
    WaitResponse   = (1 << (FirstFlagIndex + 0)),
    Synchronous    = (1 << (FirstFlagIndex + 1)),
    Idempotent     = (1 << (FirstFlagIndex + 2)),
    OneShotSend    = (1 << (FirstFlagIndex + 3)),
    StartedSend    = (1 << (FirstFlagIndex + 4)),
    DoneSend       = (1 << (FirstFlagIndex + 5)),
    Canceled       = (1 << (FirstFlagIndex + 6)),
    Response       = (1 << (FirstFlagIndex + 7)),
    OnPeer         = (1 << (FirstFlagIndex + 8)),
    BackOnSender   = (1 << (FirstFlagIndex + 9)),
};

//using MessageFlagsValueT = std::underlying_type<MessageFlags>::type;

inline MessageFlagsValueT operator|(const MessageFlagsValueT _v, const MessageFlags _f)
{
    return _v | static_cast<MessageFlagsValueT>(_f);
}

inline MessageFlagsValueT operator&(const MessageFlagsValueT _v, const MessageFlags _f)
{
    return _v & static_cast<MessageFlagsValueT>(_f);
}

inline MessageFlagsValueT& operator|=(MessageFlagsValueT& _rv, const MessageFlags _f)
{
    _rv |= static_cast<MessageFlagsValueT>(_f);
    return _rv;
}

inline MessageFlagsValueT operator|(const MessageFlags _f1, const MessageFlags _f2)
{
    return static_cast<MessageFlagsValueT>(_f1) | static_cast<MessageFlagsValueT>(_f2);
}

struct MessageHeader{
    using FlagsT = MessageFlagsValueT;
    
    static FlagsT state_flags(const FlagsT _flags)
    {
        return _flags & (MessageFlags::OnPeer | MessageFlags::BackOnSender);
    }
    
    MessageHeader():flags_(0){}
    
    MessageHeader(
        const MessageHeader &_rmsgh
    ):sender_request_id_(_rmsgh.sender_request_id_)
      , recipient_request_id_(_rmsgh.recipient_request_id_)
      , flags_(state_flags(_rmsgh.flags_)){}
    
    MessageHeader& operator=(MessageHeader &&_umh){
        sender_request_id_ = _umh.sender_request_id_;
        recipient_request_id_ = _umh.recipient_request_id_;
        flags_ = _umh.flags_;
        url_ = std::move(_umh.url_);
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

            _rs.push(_rctx.message_url, "url");
            _rs.pushCross(_rctx.message_flags, "flags");
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

    static bool is_synchronous(const FlagsT _flags)
    {
        return (_flags & MessageFlags::Synchronous) != 0;
    }
    static bool is_asynchronous(const FlagsT _flags)
    {
        return (_flags & MessageFlags::Synchronous) == 0;
    }

    static bool is_waiting_response(const FlagsT _flags)
    {
        return (_flags & MessageFlags::WaitResponse) != 0;
    }

    static bool is_request(const FlagsT _flags)
    {
        return is_waiting_response(_flags);
    }

    static bool is_idempotent(const FlagsT _flags)
    {
        return (_flags & MessageFlags::Idempotent) != 0;
    }

    static bool is_started_send(const FlagsT _flags)
    {
        return (_flags & MessageFlags::StartedSend) != 0;
    }

    static bool is_done_send(const FlagsT _flags)
    {
        return (_flags & MessageFlags::DoneSend) != 0;
    }

    static bool is_canceled(const FlagsT _flags)
    {
        return (_flags & MessageFlags::Canceled) != 0;
    }

    static bool is_one_shot(const FlagsT _flags)
    {
        return (_flags & MessageFlags::OneShotSend) != 0;
    }

    static bool is_response(const FlagsT _flags)
    {
        return (_flags & MessageFlags::Response) != 0;
    }

    static bool is_on_sender(const FlagsT _flags)
    {
        return !is_on_peer(_flags) && !is_back_on_sender(_flags);
    }

    static bool is_on_peer(const FlagsT _flags)
    {
        return (_flags & MessageFlags::OnPeer) != 0;
    }

    static bool is_back_on_sender(const FlagsT _flags)
    {
        return (_flags & MessageFlags::BackOnSender) != 0;
    }

    static bool is_back_on_peer(const FlagsT _flags)
    {
        return is_on_peer(_flags) && is_back_on_sender(_flags);
    }

    static FlagsT clear_state_flags(const FlagsT _flags)
    {
        return _flags & (~(MessageFlags::OnPeer | MessageFlags::BackOnSender));
    }

    static FlagsT state_flags(const FlagsT _flags)
    {
        return _flags & (MessageFlags::OnPeer | MessageFlags::BackOnSender);
    }

    static FlagsT update_state_flags(const FlagsT _flags)
    {
        if (is_on_sender(_flags)) {
            return _flags | MessageFlags::OnPeer;
        } else if (is_back_on_peer(_flags)) {
            return clear_state_flags(_flags);
        } else if (is_on_peer(_flags)) {
            return (_flags | MessageFlags::BackOnSender) & ~(0 | MessageFlags::OnPeer);
        } else /* if(is_back_on_sender(_flags))*/ {
            return _flags | MessageFlags::OnPeer;
        }
    }

    Message()
    {
    }

    Message(Message const& _rmsg):header_(_rmsg.header_){}

    virtual ~Message();

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
        header_.flags_ = clear_state_flags(header_.flags_);
    }

    FlagsT flags() const
    {
        return header_.flags_;
    }

    template <class S, class T>
    static void solidSerialize(S& _rs, T& _rt, const char* _name)
    {
        //here we do only pushes so we can have access to context
        //using the above "serialize" function
        _rs.push(_rt, _name);
    }

    RequestId const& senderRequestId() const
    {
        return header_.sender_request_id_;
    }

private:
    friend class Service;
    friend class TestEntryway;
    friend class Connection;

    RequestId const& requestId() const
    {
        return header_.recipient_request_id_;
    }
    
    void header(MessageHeader &&_umh){
        header_ = std::move(_umh);
    }
    
private:
    MessageHeader   header_;
};

using MessagePointerT = std::shared_ptr<Message>;

} //namespace mpipc
} //namespace frame
} //namespace solid

#endif
