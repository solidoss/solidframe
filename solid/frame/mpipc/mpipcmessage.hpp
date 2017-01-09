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

#include "solid/system/function.hpp"
#include "solid/system/common.hpp"

#include "solid/frame/mpipc/mpipccontext.hpp"

#include <memory>
#include <type_traits>

namespace solid{
namespace frame{
namespace mpipc{

class Service;
class Connection;

enum struct MessageFlags: ulong{
    FirstFlagIndex  = 0, //for rezerved flags
    WaitResponse    = (1<<(FirstFlagIndex +  0)),
    Synchronous     = (1<<(FirstFlagIndex +  1)),
    Idempotent      = (1<<(FirstFlagIndex +  2)),
    OneShotSend     = (1<<(FirstFlagIndex +  3)),
    StartedSend     = (1<<(FirstFlagIndex +  4)),
    DoneSend        = (1<<(FirstFlagIndex +  5)),
    Canceled        = (1<<(FirstFlagIndex +  6)),
    Response        = (1<<(FirstFlagIndex +  7)),
};

using MessageFlagsValueT = std::underlying_type<MessageFlags>::type;

inline MessageFlagsValueT operator|(const MessageFlagsValueT _v, const MessageFlags _f){
    return _v | static_cast<MessageFlagsValueT>(_f);
}

inline MessageFlagsValueT operator&(const MessageFlagsValueT _v, const MessageFlags _f){
    return _v & static_cast<MessageFlagsValueT>(_f);
}

inline MessageFlagsValueT& operator|=(MessageFlagsValueT & _rv, const MessageFlags _f){
    _rv |= static_cast<MessageFlagsValueT>(_f);
    return _rv;
}

inline MessageFlagsValueT operator|(const MessageFlags _f1, const MessageFlags _f2){
    return static_cast<MessageFlagsValueT>(_f1) | static_cast<MessageFlagsValueT>(_f2);
}


struct Message: std::enable_shared_from_this<Message>{

    static bool is_synchronous(const ulong _flags){
        return (_flags & MessageFlags::Synchronous) != 0;
    }
    static bool is_asynchronous(const ulong _flags){
        return (_flags & MessageFlags::Synchronous) == 0;
    }

    static bool is_waiting_response(const ulong _flags){
        return (_flags & MessageFlags::WaitResponse) != 0;
    }

    static bool is_request(const ulong _flags){
        return is_waiting_response(_flags);
    }

    static bool is_idempotent(const ulong _flags){
        return (_flags & MessageFlags::Idempotent) != 0;
    }

    static bool is_started_send(const ulong _flags){
        return (_flags & MessageFlags::StartedSend) != 0;
    }

    static bool is_done_send(const ulong _flags){
        return (_flags & MessageFlags::DoneSend) != 0;
    }

    static bool is_canceled(const ulong _flags){
        return (_flags & MessageFlags::Canceled) != 0;
    }

    static bool is_one_shot(const ulong _flags){
        return (_flags & MessageFlags::OneShotSend) != 0;
    }

    static bool is_response(const ulong _flags){
        return (_flags & MessageFlags::Response) != 0;
    }

    Message(uint8_t _state = 0):stt(_state){}
    Message(Message const &_rmsg): requid(_rmsg.requid), stt(_rmsg.stt){}

    virtual ~Message();

    bool isOnSender()const{
        return stt == 0;
    }
    bool isOnPeer()const{
        return stt == 1;
    }
    bool isBackOnSender()const{
        return stt == 2;
    }

    uint8_t state()const{
        return stt;
    }

    void clearState(){
        stt = 0;
    }

    RequestId const& requestId()const{
        return requid;
    }

    template <class S>
    void serialize(S &_rs, frame::mpipc::ConnectionContext &_rctx){
        if(S::IsSerializer){
            //because a message can be sent to multiple destinations (usign DynamicPointer)
            //on serialization we cannot use/modify the values stored by ipc::Message
            //so, we'll use ones store in the context. Because the context is volatile
            //we'll store as values.
            _rs.pushCrossValue(_rctx.request_id.index, "requid_idx");
            _rs.pushCrossValue(_rctx.request_id.unique, "requid_idx");
            _rs.pushValue(_rctx.message_state, "state");
        }else{
            _rs.pushCross(requid.index, "requid_idx");
            _rs.pushCross(requid.unique, "requid_uid");
            _rs.push(stt, "state");
        }
    }


    template <class S, class T>
    static void serialize(S &_rs, T &_rt, const char *_name){
        //here we do only pushes so we can have access to context
        //using the above "serialize" function
        _rs.push(_rt, _name);
        _rs.push(static_cast<Message&>(_rt), "message_base");
    }

private:
    friend class Service;
    friend class TestEntryway;
    friend class Connection;

    void adjustState(){
        if(stt == 3) stt = 0;
    }
private:
    RequestId   requid;
    uint8_t     stt;
};

using MessagePointerT = std::shared_ptr<Message>;

}//namespace mpipc
}//namespace frame
}//namespace solid

#endif
