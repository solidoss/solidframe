// solid/frame/ipc/src/ipcmessagewriter.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <vector>

#include "solid/system/common.hpp"
#include "solid/system/error.hpp"
#include "solid/system/function.hpp"

#include "mpipcutility.hpp"
#include "solid/frame/mpipc/mpipcprotocol.hpp"

#include "solid/utility/flags.hpp"
#include "solid/utility/innerlist.hpp"

namespace solid {
namespace frame {
namespace mpipc {

struct WriteBuffer {
    WriteBuffer(char* _data = nullptr, size_t _size = -1)
        : data_(_data)
        , size_(_size)
    {
    }
    char*  data() const noexcept { return data_; }
    size_t size() const noexcept { return size_; }

    void resize(const size_t _size) noexcept { size_ = _size; }

    bool empty() const noexcept { return size_ == 0; }

    char* end() const noexcept { return data_ + size_; }

    void reset(char* _data = nullptr, size_t _size = -1) noexcept
    {
        data_ = _data;
        size_ = _size;
    }

private:
    char*  data_;
    size_t size_;
};

class MessageWriter {
public:
    struct Sender {
        WriterConfiguration const& rconfig_;
        Protocol const&            rproto_;
        ConnectionContext&         rctx_;
        
        Sender(
            WriterConfiguration const& _rconfig,
            Protocol const&            _rproto,
            ConnectionContext&         _rctx
        ):rconfig_(_rconfig), rproto_(_rproto), rctx_(_rctx){}
        
        WriterConfiguration const& configuration()const{return rconfig_;}
        Protocol const& protocol()const{return rproto_;}
        ConnectionContext& context()const{return rctx_;}
        
        
        virtual ~Sender() {}

        virtual ErrorConditionT completeMessage(MessageBundle& /*_rmsgbundle*/, MessageId const& /*_rmsgid*/) = 0;
    };

    using VisitFunctionT = SOLID_FUNCTION<void(
        MessageBundle& /*_rmsgbundle*/,
        MessageId const& /*_rmsgid*/
        )>;

    using CompleteFunctionT = SOLID_FUNCTION<ErrorConditionT(
        MessageBundle& /*_rmsgbundle*/,
        MessageId const& /*_rmsgid*/
        )>;

    enum PrintWhat {
        PrintInnerListsE,
    };

    enum struct WriteFlagsE {
        ShouldSendKeepAlive,
        LastFlag
    };

    using WriteFlagsT      = Flags<WriteFlagsE>;
    using RequestIdVectorT = std::vector<RequestId>;

    MessageWriter();
    ~MessageWriter();

    bool enqueue(
        WriterConfiguration const& _rconfig,
        MessageBundle&             _rmsgbundle,
        MessageId const&           _rpool_msg_id,
        MessageId&                 _rconn_msg_id);

    bool cancel(
        MessageId const& _rmsguid,
        MessageBundle&   _rmsgbundle,
        MessageId&       _rpool_msg_id);

    MessagePointerT fetchRequest(MessageId const& _rmsguid) const;

    bool cancelOldest(
        MessageBundle& _rmsgbundle,
        MessageId&     _rpool_msg_id);

    ErrorConditionT write(
        WriteBuffer&               _rbuffer,
        const WriteFlagsT&         _flags,
        uint8_t&                   _rackd_buf_count,
        RequestIdVectorT&          _cancel_remote_msg_vec,
        uint8_t&                   _rrelay_free_count,
        Sender&                    _rsender);

    bool empty() const;

    bool full(WriterConfiguration const& _rconfig) const;

    void prepare(WriterConfiguration const& _rconfig);
    void unprepare();

    void forEveryMessagesNewerToOlder(VisitFunctionT const& _rvisit_fnc);

    void print(std::ostream& _ros, const PrintWhat _what) const;

private:
    enum {
        InnerLinkStatus = 0,
        InnerLinkOrder,
        InnerLinkCount
    };

    enum struct InnerStatus {
        Invalid,
        Pending = 1,
        Sending,
        Waiting,
        Completing
    };

    struct MessageStub : InnerNode<InnerLinkCount> {

        enum struct StateE : uint8_t {
            WriteStart,
            WriteHead,
            WriteBody,
            RelayedStart,
            RelayedHead,
            RelayedBody,
            Canceled,
        };

        MessageBundle      msgbundle_;
        uint32_t           unique_;
        size_t             packet_count_;
        SerializerPointerT serializer_ptr_;
        MessageId          pool_msg_id_;
        StateE             state_;

        MessageStub(
            MessageBundle& _rmsgbundle)
            : msgbundle_(std::move(_rmsgbundle))
            , packet_count_(0)
            , state_(StateE::WriteStart)
        {
        }

        MessageStub()
            : unique_(0)
            , packet_count_(0)
            , state_(StateE::WriteStart)
        {
        }

        MessageStub(
            MessageStub&& _rmsgstub)
            : InnerNode<InnerLinkCount>(std::move(_rmsgstub))
            , msgbundle_(std::move(_rmsgstub.msgbundle_))
            , unique_(_rmsgstub.unique_)
            , packet_count_(_rmsgstub.packet_count_)
            , serializer_ptr_(std::move(_rmsgstub.serializer_ptr_))
            , pool_msg_id_(_rmsgstub.pool_msg_id_)
            , state_(_rmsgstub.state_)
        {
        }

        void clear()
        {
            msgbundle_.clear();
            ++unique_;
            packet_count_ = 0;

            serializer_ptr_ = nullptr;

            pool_msg_id_.clear();
            state_ = StateE::WriteStart;
        }

        bool isStop() const noexcept
        {
            return not msgbundle_.message_ptr and not Message::is_canceled(msgbundle_.message_flags);
        }

        bool isRelay() const noexcept
        {
            return not msgbundle_.message_url.empty();
        }

        bool isSynchronous() const noexcept
        {
            return Message::is_synchronous(msgbundle_.message_flags);
        }

        bool isCanceled() const noexcept
        {
            return state_ == StateE::Canceled;
        }

        void cancel()
        {
            msgbundle_.message_flags.set(MessageFlagsE::Canceled);
            state_ = StateE::Canceled;
        }
    };

    using MessageVectorT          = std::vector<MessageStub>;
    using MessageOrderInnerListT  = InnerList<MessageVectorT, InnerLinkOrder>;
    using MessageStatusInnerListT = InnerList<MessageVectorT, InnerLinkStatus>;

    struct PacketOptions {
        PacketHeader::Types packet_type;
        bool                force_no_compress;
        bool                request_accept;

        PacketOptions()
            : packet_type(PacketHeader::MessageTypeE)
            , force_no_compress(false)
            , request_accept(false)
        {
        }
    };

    size_t doWritePacketData(
        char*                      _pbufbeg,
        char*                      _pbufend,
        PacketOptions&             _rpacket_options,
        bool&                      _rmore,
        const WriteFlagsT&         _flags,
        uint8_t&                   _rackd_buf_count,
        RequestIdVectorT&          _cancel_remote_msg_vec,
        uint8_t                    _relay_free_count,
        Sender&                    _rsender,
        ErrorConditionT&           _rerror);

    bool doCancel(
        const size_t   _msgidx,
        MessageBundle& _rmsgbundle,
        MessageId&     _rpool_msg_id);

    void doLocateNextWriteMessage();

    bool isSynchronousInSendingQueue() const;
    bool isAsynchronousInPendingQueue() const;
    bool isDelayedCloseInPendingQueue() const;

    void doTryMoveMessageFromPendingToWriteQueue(mpipc::Configuration const& _rconfig);

    PacketHeader::Types doPrepareMessageForSending(
        const size_t               _msgidx,
        WriterConfiguration const& _rconfig,
        Protocol const&            _rproto,
        ConnectionContext&         _rctx,
        SerializerPointerT&        _rtmp_serializer);

    void doTryCompleteMessageAfterSerialization(
        const size_t               _msgidx,
        Sender&                    _rsender,
        WriterConfiguration const& _rconfig,
        Protocol const&            _rproto,
        ConnectionContext&         _rctx,
        SerializerPointerT&        _rtmp_serializer,
        ErrorConditionT&           _rerror);

    void doUnprepareMessageStub(const size_t _msgidx);

private:
    MessageVectorT          message_vec_;
    uint32_t                current_message_type_id_;
    size_t                  current_synchronous_message_idx_;
    MessageOrderInnerListT  order_inner_list_;
    MessageStatusInnerListT write_inner_list_;
    MessageStatusInnerListT cache_inner_list_;
};

typedef std::pair<MessageWriter const&, MessageWriter::PrintWhat> MessageWriterPrintPairT;

std::ostream& operator<<(std::ostream& _ros, MessageWriterPrintPairT const& _msgwriter);

} //namespace mpipc
} //namespace frame
} //namespace solid
