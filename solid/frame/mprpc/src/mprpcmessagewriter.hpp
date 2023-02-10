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
#include "solid/utility/function.hpp"

#include "mprpcutility.hpp"
#include "solid/frame/mprpc/mprpcprotocol.hpp"

#include "solid/system/flags.hpp"
#include "solid/utility/innerlist.hpp"

namespace solid {
namespace frame {
namespace mprpc {

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
        ConnectionContext&         rconctx_;

        Sender(
            WriterConfiguration const& _rconfig,
            Protocol const&            _rproto,
            ConnectionContext&         _rconctx)
            : rconfig_(_rconfig)
            , rproto_(_rproto)
            , rconctx_(_rconctx)
        {
        }

        WriterConfiguration const& configuration() const { return rconfig_; }
        Protocol const&            protocol() const { return rproto_; }
        ConnectionContext&         context() const { return rconctx_; }

        virtual ~Sender();

        virtual ErrorConditionT completeMessage(MessageBundle& /*_rmsgbundle*/, MessageId const& /*_rmsgid*/);
        virtual void            completeRelayed(RelayData* _relay_data, MessageId const& _rmsgid);
        virtual bool            cancelMessage(MessageBundle& /*_rmsgbundle*/, MessageId const& /*_rmsgid*/);
        virtual void            cancelRelayed(RelayData* _relay_data, MessageId const& _rmsgid);
    };

    using VisitFunctionT = solid_function_t(void(
        MessageBundle& /*_rmsgbundle*/,
        MessageId const& /*_rmsgid*/
        ));

    using CompleteFunctionT = solid_function_t(ErrorConditionT(
        MessageBundle& /*_rmsgbundle*/,
        MessageId const& /*_rmsgid*/
        ));

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

    bool enqueue(
        WriterConfiguration const& _rconfig,
        RelayData*&                _rprelay_data,
        MessageId const&           _rengine_msg_id,
        MessageId&                 _rconn_msg_id,
        bool&                      _rmore);

    void cancel(MessageId const& _rmsguid, Sender& _rsender, const bool _force = false);

    MessagePointerT fetchRequest(MessageId const& _rmsguid) const;

    ResponseStateE checkResponseState(MessageId const& _rmsguid, MessageId& _rrelay_id, const bool _erase_request);

    void cancelOldest(Sender& _rsender);

    ErrorConditionT write(
        WriteBuffer&       _rbuffer,
        const WriteFlagsT& _flags,
        uint8_t&           _rackd_buf_count,
        RequestIdVectorT&  _cancel_remote_msg_vec,
        uint8_t&           _rrelay_free_count,
        Sender&            _rsender);

    bool isEmpty() const;

    bool isFull(WriterConfiguration const& _rconfig) const;

    bool canHandleMore(WriterConfiguration const& _rconfig) const;

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

    struct MessageStub : inner::Node<InnerLinkCount> {

        enum struct StateE : uint8_t {
            WriteStart,
            WriteHeadStart,
            WriteHeadContinue,
            WriteBodyStart,
            WriteBodyContinue,
            WriteWait,
            WriteCanceled,
            WriteWaitCanceled,
            RelayedStart, // add non-relayed states above
            RelayedHeadStart,
            RelayedHeadContinue,
            RelayedBody,
            RelayedWait,
            RelayedCancelRequest,
            RelayedCancel,
        };

        MessageBundle        msgbundle_;
        uint32_t             unique_;
        size_t               packet_count_;
        Serializer::PointerT serializer_ptr_;
        MessageId            pool_msg_id_;
        StateE               state_;
        RelayData*           prelay_data_; // TODO: make somehow prelay_data_ act as a const pointer as its data must not be changed by Writer
        const char*          prelay_pos_;
        size_t               relay_size_;

        MessageStub(
            MessageBundle& _rmsgbundle)
            : msgbundle_(std::move(_rmsgbundle))
            , packet_count_(0)
            , state_(StateE::WriteStart)
            , prelay_data_(nullptr)
        {
        }

        MessageStub()
            : unique_(0)
            , packet_count_(0)
            , state_(StateE::WriteStart)
            , prelay_data_(nullptr)
        {
        }

        MessageStub(
            MessageStub&& _rmsgstub)
            : inner::Node<InnerLinkCount>(std::move(_rmsgstub))
            , msgbundle_(std::move(_rmsgstub.msgbundle_))
            , unique_(_rmsgstub.unique_)
            , packet_count_(_rmsgstub.packet_count_)
            , serializer_ptr_(std::move(_rmsgstub.serializer_ptr_))
            , pool_msg_id_(_rmsgstub.pool_msg_id_)
            , state_(_rmsgstub.state_)
            , prelay_data_(nullptr)
        {
        }

        void clear()
        {
            msgbundle_.clear();
            ++unique_;
            packet_count_ = 0;

            solid_assert_log(prelay_data_ == nullptr, generic_logger);
            serializer_ptr_ = nullptr;

            pool_msg_id_.clear();
            state_ = StateE::WriteStart;
        }

        bool isHeadState() const noexcept
        {
            return state_ == StateE::WriteHeadStart || state_ == StateE::WriteHeadContinue || state_ == StateE::RelayedHeadStart || state_ == StateE::RelayedHeadContinue;
        }

        bool isStartOrHeadState() const noexcept
        {
            return state_ == StateE::WriteStart || state_ == StateE::WriteHeadStart || state_ == StateE::WriteHeadContinue || state_ == StateE::RelayedStart || state_ == StateE::RelayedHeadStart || state_ == StateE::RelayedHeadContinue;
        }

        bool isWaitResponseState() const noexcept
        {
            return state_ == StateE::WriteWait || state_ == StateE::RelayedWait;
        }

        bool isStop() const noexcept
        {
            return !msgbundle_.message_ptr && !Message::is_canceled(msgbundle_.message_flags);
        }

        bool isRelay() const noexcept
        {
            return !msgbundle_.message_url.empty() || Message::is_relayed(msgbundle_.message_flags); // TODO: optimize!!
        }

        bool isRelayed() const noexcept
        {
            return state_ >= StateE::RelayedStart;
        }

        bool isSynchronous() const noexcept
        {
            return Message::is_synchronous(msgbundle_.message_flags);
        }
    };

    using MessageVectorT          = std::vector<MessageStub>;
    using MessageOrderInnerListT  = inner::List<MessageVectorT, InnerLinkOrder>;
    using MessageStatusInnerListT = inner::List<MessageVectorT, InnerLinkStatus>;

    struct PacketOptions {
        bool force_no_compress;
        bool request_accept;

        PacketOptions()
            : force_no_compress(false)
            , request_accept(false)
        {
        }
    };

    size_t doWritePacketData(
        char*             _pbufbeg,
        char*             _pbufend,
        PacketOptions&    _rpacket_options,
        uint8_t&          _rackd_buf_count,
        RequestIdVectorT& _cancel_remote_msg_vec,
        uint8_t           _relay_free_count,
        Sender&           _rsender,
        ErrorConditionT&  _rerror);

    void doCancel(const size_t _msgidx, Sender& _rsender, const bool _force = false);

    bool isSynchronousInSendingQueue() const;
    bool isAsynchronousInPendingQueue() const;
    bool isDelayedCloseInPendingQueue() const;

    bool doFindEligibleMessage(Sender& _rsender, const bool _can_send_relay, const size_t _size);

    void doTryMoveMessageFromPendingToWriteQueue(mprpc::Configuration const& _rconfig);

    char* doWriteMessageHead(
        char*                        _pbufpos,
        char*                        _pbufend,
        const size_t                 _msgidx,
        PacketOptions&               _rpacket_options,
        Sender&                      _rsender,
        const PacketHeader::CommandE _cmd,
        ErrorConditionT&             _rerror);

    char* doWriteMessageBody(
        char*            _pbufpos,
        char*            _pbufend,
        const size_t     _msgidx,
        PacketOptions&   _rpacket_options,
        Sender&          _rsender,
        ErrorConditionT& _rerror);

    char* doWriteRelayedHead(
        char*                        _pbufpos,
        char*                        _pbufend,
        const size_t                 _msgidx,
        PacketOptions&               _rpacket_options,
        Sender&                      _rsender,
        const PacketHeader::CommandE _cmd,
        ErrorConditionT&             _rerror);

    char* doWriteRelayedBody(
        char*            _pbufpos,
        char*            _pbufend,
        const size_t     _msgidx,
        PacketOptions&   _rpacket_options,
        Sender&          _rsender,
        ErrorConditionT& _rerror);

    char* doWriteMessageCancel(
        char*            _pbufpos,
        char*            _pbufend,
        const size_t     _msgidx,
        PacketOptions&   _rpacket_options,
        Sender&          _rsender,
        ErrorConditionT& _rerror);
    char* doWriteRelayedCancel(
        char*            _pbufpos,
        char*            _pbufend,
        const size_t     _msgidx,
        PacketOptions&   _rpacket_options,
        Sender&          _rsender,
        ErrorConditionT& _rerror);
    char* doWriteRelayedCancelRequest(
        char*            _pbufpos,
        char*            _pbufend,
        const size_t     _msgidx,
        PacketOptions&   _rpacket_options,
        Sender&          _rsender,
        ErrorConditionT& _rerror);
    void doTryCompleteMessageAfterSerialization(
        const size_t     _msgidx,
        Sender&          _rsender,
        ErrorConditionT& _rerror);

    void doUnprepareMessageStub(const size_t _msgidx);
    void doWriteQueuePushBack(const size_t _msgidx, const int _line);
    void doWriteQueueErase(const size_t _msgidx, const int _line);

    void                 cache(Serializer::PointerT& _ser);
    Serializer::PointerT createSerializer(Sender& _sender);

private:
    MessageVectorT          message_vec_;
    uint32_t                current_message_type_id_;
    size_t                  write_queue_sync_index_;
    size_t                  write_queue_back_index_;
    size_t                  write_queue_async_count_;
    size_t                  write_queue_direct_count_;
    MessageOrderInnerListT  order_inner_list_;
    MessageStatusInnerListT write_inner_list_;
    MessageStatusInnerListT cache_inner_list_;
    Serializer::PointerT    serializer_stack_top_;
};

typedef std::pair<MessageWriter const&, MessageWriter::PrintWhat> MessageWriterPrintPairT;

std::ostream& operator<<(std::ostream& _ros, MessageWriterPrintPairT const& _msgwriter);

//-----------------------------------------------------------------------------
inline bool MessageWriter::isFull(WriterConfiguration const& _rconfig) const
{
    return write_inner_list_.size() >= _rconfig.max_message_count_multiplex;
}
//-----------------------------------------------------------------------------
inline bool MessageWriter::isEmpty() const
{
    return order_inner_list_.empty();
}
//-----------------------------------------------------------------------------
inline bool MessageWriter::canHandleMore(WriterConfiguration const& _rconfig) const
{
    return write_inner_list_.size() < _rconfig.max_message_count_multiplex;
}
//-----------------------------------------------------------------------------

} // namespace mprpc
} // namespace frame
} // namespace solid
