// solid/frame/ipc/src/ipcmessagereader.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "mprpcmessagewriter.hpp"
#include "mprpcutility.hpp"

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcerror.hpp"

#include "solid/system/cassert.hpp"
#include "solid/system/log.hpp"

namespace solid {
namespace frame {
namespace mprpc {
namespace {
const LoggerT logger("solid::frame::mprpc::writer");
}
//-----------------------------------------------------------------------------
MessageWriter::MessageWriter()
    : current_message_type_id_(InvalidIndex())
    , write_queue_sync_index_(InvalidIndex())
    , write_queue_back_index_(InvalidIndex())
    , write_queue_async_count_(0)
    , write_queue_direct_count_(0)
    , order_inner_list_(message_vec_)
    , write_inner_list_(message_vec_)
    , cache_inner_list_(message_vec_)
{
}
//-----------------------------------------------------------------------------
MessageWriter::~MessageWriter() {}
//-----------------------------------------------------------------------------
void MessageWriter::prepare(WriterConfiguration const& _rconfig)
{
    //WARNING: message_vec_ MUST NOT be resized later
    //as it would interfere with pointers stored in serializer.
    message_vec_.reserve(_rconfig.max_message_count_multiplex + _rconfig.max_message_count_response_wait);
    message_vec_.resize(message_vec_.capacity());

    for (size_t i = 0; i < message_vec_.size(); ++i) {
        cache_inner_list_.pushBack(i);
    }
}
//-----------------------------------------------------------------------------
void MessageWriter::unprepare()
{
}
//-----------------------------------------------------------------------------
bool MessageWriter::full(WriterConfiguration const& _rconfig) const
{
    return write_inner_list_.size() >= _rconfig.max_message_count_multiplex;
}
//-----------------------------------------------------------------------------

void MessageWriter::doWriteQueuePushBack(const size_t _msgidx, const int _line)
{
    if (write_inner_list_.size() <= 1) {
        //"(size() <= 1)" because write_queue_back_index_ may be -1 when write_inner_list_.size() == 1
        write_inner_list_.pushBack(_msgidx);
        write_queue_back_index_ = _msgidx;
    } else {
        solid_assert_log(write_queue_back_index_ != InvalidIndex(), logger, " msgidx = " << _msgidx << "  write_inner_list.size = " << write_inner_list_.size());
        write_inner_list_.insertAfter(write_queue_back_index_, _msgidx);
        write_queue_back_index_ = _msgidx;
    }
    MessageStub& rmsgstub(message_vec_[_msgidx]);

    solid_log(logger, Info, this << " code line = " << _line << " idx = " << _msgidx << " is_relay = " << rmsgstub.isRelay());
    if (!rmsgstub.isRelay()) {
        ++write_queue_direct_count_;
    }
    if (!rmsgstub.isSynchronous()) {
        ++write_queue_async_count_;
    }
}
//-----------------------------------------------------------------------------
void MessageWriter::doWriteQueueErase(const size_t _msgidx, const int _line)
{
    MessageStub& rmsgstub(message_vec_[_msgidx]);
    solid_log(logger, Info, this << " code line = " << _line << " idx = " << _msgidx << " is_relay = " << rmsgstub.isRelay());
    if (!rmsgstub.isRelay()) {
        --write_queue_direct_count_;
    }
    if (!rmsgstub.isSynchronous()) {
        --write_queue_async_count_;
    }

    if (_msgidx == write_queue_sync_index_) {
        write_queue_sync_index_ = InvalidIndex();
    }

    if (_msgidx == write_queue_back_index_) {
        write_queue_back_index_ = write_inner_list_.nextIndex(_msgidx);
        if (write_queue_back_index_ == InvalidIndex()) {
            write_queue_back_index_ = write_inner_list_.backIndex();
        }
    }

    write_inner_list_.erase(_msgidx);
}
//-----------------------------------------------------------------------------

bool MessageWriter::enqueue(
    WriterConfiguration const& _rconfig,
    MessageBundle&             _rmsgbundle,
    MessageId const&           _rpool_msg_id,
    MessageId&                 _rconn_msg_id)
{

    //see if we can accept the message
    if (full(_rconfig) || cache_inner_list_.empty()) {
        return false;
    }

    //see if we have too many messages waiting for responses
    if (
        Message::is_awaiting_response(_rmsgbundle.message_flags) && ((order_inner_list_.size() - write_inner_list_.size()) >= _rconfig.max_message_count_response_wait)) {
        return false;
    }

    solid_assert_log(_rmsgbundle.message_ptr.get(), logger);

    //clear all disrupting flags
    _rmsgbundle.message_flags.reset(MessageFlagsE::StartedSend).reset(MessageFlagsE::DoneSend);

    const size_t idx = cache_inner_list_.popFront();
    MessageStub& rmsgstub(message_vec_[idx]);

    rmsgstub.msgbundle_               = std::move(_rmsgbundle);
    rmsgstub.pool_msg_id_             = _rpool_msg_id;
    rmsgstub.msgbundle_.message_flags = Message::update_state_flags(Message::clear_state_flags(rmsgstub.msgbundle_.message_flags) | Message::state_flags(rmsgstub.msgbundle_.message_ptr->flags()));

    _rconn_msg_id = MessageId(idx, rmsgstub.unique_);

    order_inner_list_.pushBack(idx);
    doWriteQueuePushBack(idx, __LINE__);
    solid_log(logger, Verbose, "is_relayed = " << Message::is_relayed(rmsgstub.msgbundle_.message_ptr->flags()) << ' ' << MessageWriterPrintPairT(*this, PrintInnerListsE));

    return true;
}
//-----------------------------------------------------------------------------
bool MessageWriter::enqueue(
    WriterConfiguration const& _rconfig,
    RelayData*&                _rprelay_data,
    MessageId const&           _rengine_msg_id,
    MessageId&                 _rconn_msg_id,
    bool&                      _rmore)
{
    if (full(_rconfig)) {
        _rmore = false;
        solid_log(logger, Verbose, "");
        return false;
    }

    size_t msgidx;
    //see if we have too many messages waiting for responses

    if (_rconn_msg_id.isInvalid()) { //front message data
        solid_log(logger, Verbose, "");
        if (cache_inner_list_.empty()) {
            _rmore = false;
            solid_log(logger, Verbose, "");
            return false;
        }
        if (
            _rprelay_data->isRequest() && ((order_inner_list_.size() - write_inner_list_.size()) >= _rconfig.max_message_count_response_wait)) {
            solid_log(logger, Verbose, "");
            return false;
        }

        msgidx        = cache_inner_list_.popFront();
        _rconn_msg_id = MessageId(msgidx, message_vec_[msgidx].unique_);
        order_inner_list_.pushBack(msgidx);
    } else {
        msgidx = _rconn_msg_id.index;
        solid_assert_log(message_vec_[msgidx].unique_ == _rconn_msg_id.unique, logger);
        if (message_vec_[msgidx].unique_ != _rconn_msg_id.unique || message_vec_[msgidx].prelay_data_ != nullptr) {
            solid_log(logger, Verbose, this << " Relay Data cannot be accepted righ now for msgidx = " << msgidx);
            //the relay data cannot be accepted right now - will be tried later
            return false;
        }
    }

    solid_assert_log(_rprelay_data, logger);

    MessageStub& rmsgstub(message_vec_[msgidx]);

    if (_rprelay_data->pdata_ != nullptr) {

        solid_log(logger, Verbose, this << " " << msgidx << " relay_data.flags " << _rprelay_data->flags_ << ' ' << MessageWriterPrintPairT(*this, PrintInnerListsE));

        if (_rprelay_data->isMessageBegin()) {
            rmsgstub.state_                         = MessageStub::StateE::RelayedStart;
            _rprelay_data->pmessage_header_->flags_ = _rprelay_data->message_flags_;
        }

        rmsgstub.prelay_data_ = _rprelay_data;
        rmsgstub.prelay_pos_  = rmsgstub.prelay_data_->pdata_;
        rmsgstub.relay_size_  = rmsgstub.prelay_data_->data_size_;
        rmsgstub.pool_msg_id_ = _rengine_msg_id;
        _rprelay_data         = nullptr;

        doWriteQueuePushBack(msgidx, __LINE__);
    } else if (rmsgstub.state_ < MessageStub::StateE::RelayedWait) {
        solid_assert_log(rmsgstub.relay_size_ == 0, logger);
        solid_log(logger, Error, this << " " << msgidx << " uid = " << rmsgstub.unique_ << " state = " << (int)rmsgstub.state_);
        //called from relay engine on cancel request from the reader (RR - see mprpcrelayengine.cpp) side of the link.
        //after the current function call, the MessageStub in the RelayEngine is distroyed.
        //we need to forward the cancel on the connection
        rmsgstub.state_ = MessageStub::StateE::RelayedCancel;
        doWriteQueuePushBack(msgidx, __LINE__);
        solid_log(logger, Verbose, this << " relayedcancel msg " << msgidx);
        //we do not need the relay_data - leave it to the relay engine to delete it.
        //TODO:!!!! - we actually need the relay data for pmessage_header_->recipient_request_id_
    } else if (rmsgstub.state_ == MessageStub::StateE::RelayedWait) {
        solid_log(logger, Verbose, this << " relayedcancel erase msg " << msgidx << " state = " << (int)rmsgstub.state_);
        //do nothing - we cannot erase a message stub waiting for response
        //TODO:!!!! actually we should be able to cancel a message waiting for response
    } else {
        solid_log(logger, Verbose, this << " relayedcancel erase msg " << msgidx << " state = " << (int)rmsgstub.state_);
        order_inner_list_.erase(msgidx);
        doUnprepareMessageStub(msgidx);
    }

    return true;
}
//-----------------------------------------------------------------------------
void MessageWriter::doUnprepareMessageStub(const size_t _msgidx)
{
    MessageStub& rmsgstub(message_vec_[_msgidx]);

    rmsgstub.clear();
    cache_inner_list_.pushFront(_msgidx);
}
//-----------------------------------------------------------------------------
void MessageWriter::cancel(
    MessageId const& _rmsguid,
    Sender&          _rsender,
    const bool       _force)
{
    if (_rmsguid.isValid() && _rmsguid.index < message_vec_.size() && _rmsguid.unique == message_vec_[_rmsguid.index].unique_) {
        doCancel(_rmsguid.index, _rsender, _force);
    }
}
//-----------------------------------------------------------------------------
MessagePointerT MessageWriter::fetchRequest(MessageId const& _rmsguid) const
{
    if (_rmsguid.isValid() && _rmsguid.index < message_vec_.size() && _rmsguid.unique == message_vec_[_rmsguid.index].unique_) {
        const MessageStub& rmsgstub = message_vec_[_rmsguid.index];
        return MessagePointerT(rmsgstub.msgbundle_.message_ptr);
    }
    return MessagePointerT();
}
//-----------------------------------------------------------------------------
ResponseStateE MessageWriter::checkResponseState(MessageId const& _rmsguid, MessageId& _rrelay_id, const bool _erase_request)
{
    if (_rmsguid.isValid() && _rmsguid.index < message_vec_.size() && _rmsguid.unique == message_vec_[_rmsguid.index].unique_) {
        MessageStub& rmsgstub = message_vec_[_rmsguid.index];
        switch (rmsgstub.state_) {
        case MessageStub::StateE::WriteWait:
            return ResponseStateE::Wait;
        case MessageStub::StateE::RelayedWait:
            _rrelay_id = rmsgstub.pool_msg_id_;
            if (_erase_request) {
                order_inner_list_.erase(_rmsguid.index);
                doUnprepareMessageStub(_rmsguid.index);
            }
            return ResponseStateE::RelayedWait;
        case MessageStub::StateE::WriteCanceled:
            solid_assert_log(write_inner_list_.size(), logger);
            order_inner_list_.erase(_rmsguid.index);
            doWriteQueueErase(_rmsguid.index, __LINE__);
            doUnprepareMessageStub(_rmsguid.index);
            return ResponseStateE::Cancel;
        case MessageStub::StateE::WriteWaitCanceled:
            order_inner_list_.erase(_rmsguid.index);
            doUnprepareMessageStub(_rmsguid.index);
            return ResponseStateE::Cancel;
        default:
            solid_assert_log(false, logger);
            //solid_check(false, "Unknown state for response: "<<(int)rmsgstub.state_<<" for messageid: "<<_rmsguid);
            return ResponseStateE::Invalid;
        }
    }
    return ResponseStateE::None;
}
//-----------------------------------------------------------------------------
void MessageWriter::cancelOldest(Sender& _rsender)
{
    if (!order_inner_list_.empty()) {
        doCancel(order_inner_list_.frontIndex(), _rsender, true);
    }
}
//-----------------------------------------------------------------------------
void MessageWriter::doCancel(
    const size_t _msgidx,
    Sender&      _rsender,
    const bool   _force)
{

    solid_log(logger, Verbose, this << " " << _msgidx);

    MessageStub& rmsgstub = message_vec_[_msgidx];

    if (rmsgstub.state_ == MessageStub::StateE::WriteWaitCanceled) {
        solid_log(logger, Verbose, "" << _msgidx << " already canceled");

        if (_force) {
            order_inner_list_.erase(_msgidx);
            doUnprepareMessageStub(_msgidx);
        }
        return; //already canceled
    }

    if (rmsgstub.state_ == MessageStub::StateE::WriteCanceled) {
        solid_log(logger, Verbose, "" << _msgidx << " already canceled");

        if (_force) {
            solid_assert_log(write_inner_list_.size(), logger);
            order_inner_list_.erase(_msgidx);
            doWriteQueueErase(_msgidx, __LINE__);
            doUnprepareMessageStub(_msgidx);
        }
        return; //already canceled
    }

    if (rmsgstub.msgbundle_.message_ptr) {
        //called on explicit user request or on peer request (via reader) or on response received
        rmsgstub.msgbundle_.message_flags.set(MessageFlagsE::Canceled);
        if (_rsender.cancelMessage(rmsgstub.msgbundle_, rmsgstub.pool_msg_id_)) {
            if (rmsgstub.serializer_ptr_) {
                //the message is being sent
                rmsgstub.serializer_ptr_->clear();
                _rsender.protocol().reconfigure(*rmsgstub.serializer_ptr_, _rsender.configuration());
                rmsgstub.state_ = MessageStub::StateE::WriteCanceled;
            } else if (!_force && rmsgstub.state_ == MessageStub::StateE::WriteWait) {
                //message is waiting response
                rmsgstub.state_ = MessageStub::StateE::WriteWaitCanceled;
            } else if (rmsgstub.state_ == MessageStub::StateE::WriteWait || rmsgstub.state_ == MessageStub::StateE::WriteWaitCanceled) {
                order_inner_list_.erase(_msgidx);
                doUnprepareMessageStub(_msgidx);
            } else {
                //message is waiting to be sent
                solid_assert_log(write_inner_list_.size(), logger);
                order_inner_list_.erase(_msgidx);
                doWriteQueueErase(_msgidx, __LINE__);
                doUnprepareMessageStub(_msgidx);
            }
        } else {
            rmsgstub.msgbundle_.message_flags.reset(MessageFlagsE::Canceled);
        }
    } else {
        //usually called when reader receives a cancel request
        switch (rmsgstub.state_) {
        case MessageStub::StateE::RelayedHeadStart:
        case MessageStub::StateE::RelayedHeadContinue:
            rmsgstub.serializer_ptr_->clear();
            _rsender.protocol().reconfigure(*rmsgstub.serializer_ptr_, _rsender.configuration());
        case MessageStub::StateE::RelayedBody:
            rmsgstub.state_ = MessageStub::StateE::RelayedCancelRequest;
            _rsender.cancelRelayed(rmsgstub.prelay_data_, rmsgstub.pool_msg_id_);
            if (rmsgstub.prelay_data_ == nullptr) { //message not in write_inner_list_
                doWriteQueuePushBack(_msgidx, __LINE__);
            }
            rmsgstub.prelay_data_ = nullptr;
            break;
        case MessageStub::StateE::RelayedStart:
        //message not yet started
        case MessageStub::StateE::RelayedWait:
            //message waiting for response
            _rsender.cancelRelayed(rmsgstub.prelay_data_, rmsgstub.pool_msg_id_);
            rmsgstub.prelay_data_ = nullptr;
            order_inner_list_.erase(_msgidx);
            doUnprepareMessageStub(_msgidx);
            break;
        case MessageStub::StateE::RelayedCancelRequest:
            if (_force) {
                doWriteQueueErase(_msgidx, __LINE__);
                order_inner_list_.erase(_msgidx);
                doUnprepareMessageStub(_msgidx);
                return;
            }
        default:
            solid_assert_log(false, logger);
            return;
        }
    }
}
//-----------------------------------------------------------------------------
bool MessageWriter::empty() const
{
    return order_inner_list_.empty();
}
//-----------------------------------------------------------------------------
// Does:
// prepare message
// serialize messages on buffer
// completes messages - those that do not need wait for response
// keeps a serializer for every message that is multiplexed
// compress the filled buffer
//
// Needs:
//

ErrorConditionT MessageWriter::write(
    WriteBuffer&       _rbuffer,
    const WriteFlagsT& _flags,
    uint8_t&           _rackd_buf_count,
    RequestIdVectorT&  _cancel_remote_msg_vec,
    uint8_t&           _rrelay_free_count,
    Sender&            _rsender)
{

    char*           pbufpos = _rbuffer.data();
    char*           pbufend = _rbuffer.end();
    size_t          freesz  = _rbuffer.size();
    bool            more    = true;
    ErrorConditionT error;

    while (more && freesz >= (PacketHeader::SizeOfE + _rsender.protocol().minimumFreePacketDataSize())) {

        PacketHeader  packet_header(PacketHeader::TypeE::Data, 0, 0);
        PacketOptions packet_options;
        char*         pbufdata = pbufpos + PacketHeader::SizeOfE;
        size_t        fillsz   = doWritePacketData(pbufdata, pbufend, packet_options, _rackd_buf_count, _cancel_remote_msg_vec, _rrelay_free_count, _rsender, error);

        if (fillsz != 0u) {

            if (!packet_options.force_no_compress) {
                ErrorConditionT compress_error;
                size_t          compressed_size = _rsender.configuration().inplace_compress_fnc(pbufdata, fillsz, compress_error);

                if (compressed_size != 0u) {
                    packet_header.flags(packet_header.flags() | static_cast<uint8_t>(PacketHeader::FlagE::Compressed));
                    fillsz = compressed_size;
                } else if (!compress_error) {
                    //the buffer was not modified, we can send it uncompressed
                } else {
                    //there was an error and the inplace buffer was changed - exit with error
                    more  = false;
                    error = compress_error;
                    break;
                }
            }
            if (packet_options.request_accept) {
                solid_assert_log(_rrelay_free_count != 0, logger);
                solid_log(logger, Verbose, "send AckRequestFlagE");
                --_rrelay_free_count;
                packet_header.flags(packet_header.flags() | static_cast<uint8_t>(PacketHeader::FlagE::AckRequest));
                more = false; //do not allow multiple packets per relay buffer
            }

            solid_assert_log(static_cast<size_t>(fillsz) < static_cast<size_t>(0xffffUL), logger);

            packet_header.size(static_cast<uint32_t>(fillsz));

            pbufpos = packet_header.store(pbufpos, _rsender.protocol());
            pbufpos = pbufdata + fillsz;
            freesz  = pbufend - pbufpos;
        } else {
            more = false;
        }
    }

    if (!error && _rbuffer.data() == pbufpos) {
        if (_flags.has(WriteFlagsE::ShouldSendKeepAlive)) {
            PacketHeader packet_header(PacketHeader::TypeE::KeepAlive);
            pbufpos = packet_header.store(pbufpos, _rsender.protocol());
        }
    }
    _rbuffer.resize(pbufpos - _rbuffer.data());
    return error;
}
//-----------------------------------------------------------------------------
//NOTE:
// Objectives for doFindEligibleMessage:
// - be fast
// - try to fill up the package
// - be fair with all messages
bool MessageWriter::doFindEligibleMessage(Sender& _rsender, const bool _can_send_relay, const size_t /*_size*/)
{
    solid_log(logger, Verbose, "wq_back_index_ = " << write_queue_back_index_ << " wq_sync_index_ = " << write_queue_sync_index_ << " wq_async_count_ = " << write_queue_async_count_ << " wq_direct_count_ = " << write_queue_direct_count_ << " wq.size = " << write_inner_list_.size() << " _can_send_relay = " << _can_send_relay);
    //fail fast
    if (!_can_send_relay && write_queue_direct_count_ == 0) {
        return false;
    }
    size_t qsz             = write_inner_list_.size();
    size_t async_postponed = 0;
    while (qsz != 0u) {
        --qsz;
        const size_t msgidx   = write_inner_list_.frontIndex();
        MessageStub& rmsgstub = message_vec_[msgidx];

        solid_log(logger, Verbose, "msgidx = " << msgidx << " isHeadState = " << rmsgstub.isHeadState() << " isSynchronous = " << rmsgstub.isSynchronous() << " isRelay = " << rmsgstub.isRelay());

        if (rmsgstub.isHeadState()) {
            return true; //prevent splitting the header
        }

        if (rmsgstub.isSynchronous()) {
            if (write_queue_sync_index_ == InvalidIndex()) {
                write_queue_sync_index_ = msgidx;
            } else if (write_queue_sync_index_ == msgidx) {
            } else {
                write_inner_list_.pushBack(write_inner_list_.popFront());
                continue;
            }
        }
        if (rmsgstub.isStartOrHeadState()) {
            return true;
        }
        if (rmsgstub.isRelay()) {
            if (_can_send_relay) {
            } else {
                write_inner_list_.pushBack(write_inner_list_.popFront());
                continue;
            }
        }
        if (rmsgstub.packet_count_ < _rsender.configuration().max_message_continuous_packet_count) {

        } else {
            rmsgstub.packet_count_ = 0;
            if (rmsgstub.isSynchronous() && write_queue_async_count_ == 0) {
                //no async message in queue - continue with the current synchronous message
            } else if (write_queue_async_count_ > (async_postponed + 1)) { //we do not want to postpone all async messages
                write_inner_list_.pushBack(write_inner_list_.popFront());
                ++async_postponed;
                continue;
            }
        }
        solid_log(logger, Verbose, "FOUND eligible message - " << msgidx);
        return true;
    }

    solid_log(logger, Info, this << " NO eligible message in a queue of " << write_inner_list_.size() << " wq_back_index_ = " << write_queue_back_index_ << " wq_sync_index_ = " << write_queue_sync_index_ << " wq_async_count_ = " << write_queue_async_count_ << " wq_direct_count_ = " << write_queue_direct_count_ << " wq.size = " << write_inner_list_.size() << " _can_send_relay = " << _can_send_relay);
    return false;
}
//-----------------------------------------------------------------------------
// we have three types of messages:
// - direct: serialized onto buffer
// - relay: serialized onto a relay buffer (one that needs confirmation)
// - relayed: copyed onto the output buffer (no serialization)

size_t MessageWriter::doWritePacketData(
    char*             _pbufbeg,
    char*             _pbufend,
    PacketOptions&    _rpacket_options,
    uint8_t&          _rackd_buf_count,
    RequestIdVectorT& _cancel_remote_msg_vec,
    uint8_t           _relay_free_count,
    Sender&           _rsender,
    ErrorConditionT&  _rerror)
{
    char* pbufpos = _pbufbeg;

    if (_rackd_buf_count != 0u) {
        solid_log(logger, Verbose, this << " stored ackd_buf_count = " << (int)_rackd_buf_count);
        uint8_t cmd      = static_cast<uint8_t>(PacketHeader::CommandE::AckdCount);
        pbufpos          = _rsender.protocol().storeValue(pbufpos, cmd);
        pbufpos          = _rsender.protocol().storeValue(pbufpos, _rackd_buf_count);
        _rackd_buf_count = 0;
    }

    while (!_cancel_remote_msg_vec.empty() && static_cast<size_t>(_pbufend - pbufpos) >= _rsender.protocol().minimumFreePacketDataSize()) {
        solid_log(logger, Verbose, this << " send CancelRequest " << _cancel_remote_msg_vec.back());
        uint8_t cmd = static_cast<uint8_t>(PacketHeader::CommandE::CancelRequest);
        pbufpos     = _rsender.protocol().storeValue(pbufpos, cmd);
        pbufpos     = _rsender.protocol().storeCrossValue(pbufpos, _pbufend - pbufpos, _cancel_remote_msg_vec.back().index);
        solid_check_log(pbufpos != nullptr, logger, "fail store cross value");
        pbufpos = _rsender.protocol().storeCrossValue(pbufpos, _pbufend - pbufpos, _cancel_remote_msg_vec.back().unique);
        solid_check_log(pbufpos != nullptr, logger, "fail store cross value");
        _cancel_remote_msg_vec.pop_back();
    }

    while (
        !_rerror && static_cast<size_t>(_pbufend - pbufpos) >= _rsender.protocol().minimumFreePacketDataSize() && doFindEligibleMessage(_rsender, _relay_free_count != 0, _pbufend - pbufpos)) {
        const size_t msgidx = write_inner_list_.frontIndex();

        PacketHeader::CommandE cmd = PacketHeader::CommandE::Message;

        solid_log(logger, Verbose, this << " msgidx = " << msgidx << " state " << (int)message_vec_[msgidx].state_);

        switch (message_vec_[msgidx].state_) {
        case MessageStub::StateE::WriteStart: {
            MessageStub& rmsgstub = message_vec_[msgidx];

            rmsgstub.serializer_ptr_ = createSerializer(_rsender);

            rmsgstub.msgbundle_.message_flags.set(MessageFlagsE::StartedSend);

            rmsgstub.state_ = MessageStub::StateE::WriteHeadStart;

            solid_log(logger, Info, this << " message header url: " << rmsgstub.msgbundle_.message_url << " isRelay = " << rmsgstub.isRelay());

            cmd = PacketHeader::CommandE::NewMessage;
        }
        case MessageStub::StateE::WriteHeadStart:
        case MessageStub::StateE::WriteHeadContinue:
            pbufpos = doWriteMessageHead(pbufpos, _pbufend, msgidx, _rpacket_options, _rsender, cmd, _rerror);
            break;
        case MessageStub::StateE::WriteBodyStart:
        case MessageStub::StateE::WriteBodyContinue:
            pbufpos = doWriteMessageBody(pbufpos, _pbufend, msgidx, _rpacket_options, _rsender, _rerror);
            break;
        case MessageStub::StateE::WriteWait:
            solid_throw_log(logger, "Invalid state for write queue - WriteWait");
            break;
        case MessageStub::StateE::WriteCanceled:
            pbufpos = doWriteMessageCancel(pbufpos, _pbufend, msgidx, _rpacket_options, _rsender, _rerror);
            break;
        case MessageStub::StateE::RelayedStart: {
            MessageStub& rmsgstub = message_vec_[msgidx];

            rmsgstub.serializer_ptr_ = createSerializer(_rsender);

            rmsgstub.state_ = MessageStub::StateE::RelayedHeadStart;

            solid_log(logger, Verbose, this << " message header url: " << rmsgstub.prelay_data_->pmessage_header_->url_);

            cmd = PacketHeader::CommandE::NewMessage;
        }
        case MessageStub::StateE::RelayedHeadStart:
        case MessageStub::StateE::RelayedHeadContinue:
            pbufpos = doWriteRelayedHead(pbufpos, _pbufend, msgidx, _rpacket_options, _rsender, cmd, _rerror);
            break;
        case MessageStub::StateE::RelayedBody:
            pbufpos = doWriteRelayedBody(pbufpos, _pbufend, msgidx, _rpacket_options, _rsender, _rerror);
            break;
        case MessageStub::StateE::RelayedWait:
            solid_throw_log(logger, "Invalid state for write queue - RelayedWait");
            break;
        case MessageStub::StateE::RelayedCancelRequest:
            pbufpos = doWriteRelayedCancelRequest(pbufpos, _pbufend, msgidx, _rpacket_options, _rsender, _rerror);
            break;
        case MessageStub::StateE::RelayedCancel:
            pbufpos = doWriteRelayedCancel(pbufpos, _pbufend, msgidx, _rpacket_options, _rsender, _rerror);
            break;
        default:
            //solid_check(false, "message state not handled: "<<(int)message_vec_[msgidx].state_<<" for message "<<msgidx);
            solid_assert_log(false, logger);
            break;
        }
    } //while

    solid_log(logger, Verbose, this << " write_q_size " << write_inner_list_.size() << " order_q_size " << order_inner_list_.size());

    return pbufpos - _pbufbeg;
}
//-----------------------------------------------------------------------------
char* MessageWriter::doWriteMessageHead(
    char*        _pbufpos,
    char*        _pbufend,
    const size_t _msgidx,
    PacketOptions& /*_rpacket_options*/,
    Sender&                      _rsender,
    const PacketHeader::CommandE _cmd,
    ErrorConditionT&             _rerror)
{

    uint8_t cmd = static_cast<uint8_t>(_cmd);
    _pbufpos    = _rsender.protocol().storeValue(_pbufpos, cmd);

    _pbufpos = _rsender.protocol().storeCrossValue(_pbufpos, _pbufend - _pbufpos, static_cast<uint32_t>(_msgidx));
    solid_check_log(_pbufpos != nullptr, logger, "fail store cross value");

    char* psizepos = _pbufpos;
    _pbufpos       = _rsender.protocol().storeValue(psizepos, static_cast<uint16_t>(0));

    MessageStub& rmsgstub             = message_vec_[_msgidx];
    rmsgstub.msgbundle_.message_flags = Message::update_state_flags(Message::clear_state_flags(rmsgstub.msgbundle_.message_flags) | Message::state_flags(rmsgstub.msgbundle_.message_ptr->flags()));

    _rsender.context().request_id.index  = static_cast<uint32_t>(_msgidx + 1);
    _rsender.context().request_id.unique = rmsgstub.unique_;
    _rsender.context().message_flags     = rmsgstub.msgbundle_.message_flags;
    _rsender.context().pmessage_url      = &rmsgstub.msgbundle_.message_url;

    const ptrdiff_t rv = rmsgstub.state_ == MessageStub::StateE::WriteHeadStart ? rmsgstub.serializer_ptr_->run(_rsender.context(), _pbufpos, _pbufend - _pbufpos, rmsgstub.msgbundle_.message_ptr->header_) : rmsgstub.serializer_ptr_->run(_rsender.context(), _pbufpos, _pbufend - _pbufpos);
    rmsgstub.state_    = MessageStub::StateE::WriteHeadContinue;

    if (rv >= 0) {
        _rsender.protocol().storeValue(psizepos, static_cast<uint16_t>(rv));
        solid_log(logger, Info, this << " stored message header with index = " << _msgidx << " and size = " << rv);

        if (rmsgstub.serializer_ptr_->empty()) {
            //we've just finished serializing header
            rmsgstub.state_ = MessageStub::StateE::WriteBodyStart;
        }
        _pbufpos += rv;
    } else {
        _rerror = rmsgstub.serializer_ptr_->error();
    }

    return _pbufpos;
}
//-----------------------------------------------------------------------------
char* MessageWriter::doWriteMessageBody(
    char*            _pbufpos,
    char*            _pbufend,
    const size_t     _msgidx,
    PacketOptions&   _rpacket_options,
    Sender&          _rsender,
    ErrorConditionT& _rerror)
{
    char* pcmdpos = nullptr;

    pcmdpos = _pbufpos;
    _pbufpos += 1;

    _pbufpos = _rsender.protocol().storeCrossValue(_pbufpos, _pbufend - _pbufpos, static_cast<uint32_t>(_msgidx));
    solid_check_log(_pbufpos != nullptr, logger, "fail store cross value");

    char* psizepos = _pbufpos;
    _pbufpos       = _rsender.protocol().storeValue(psizepos, static_cast<uint16_t>(0));
    uint8_t cmd    = static_cast<uint8_t>(PacketHeader::CommandE::Message);

    MessageStub& rmsgstub = message_vec_[_msgidx];

    _rsender.context().request_id.index  = static_cast<uint32_t>(_msgidx + 1);
    _rsender.context().request_id.unique = rmsgstub.unique_;
    _rsender.context().message_flags     = rmsgstub.msgbundle_.message_flags;
    _rsender.context().pmessage_url      = &rmsgstub.msgbundle_.message_url;

    const ptrdiff_t rv = rmsgstub.state_ == MessageStub::StateE::WriteBodyStart ? rmsgstub.serializer_ptr_->run(_rsender.context(), _pbufpos, _pbufend - _pbufpos, rmsgstub.msgbundle_.message_ptr, rmsgstub.msgbundle_.message_type_id) : rmsgstub.serializer_ptr_->run(_rsender.context(), _pbufpos, _pbufend - _pbufpos);
    rmsgstub.state_    = MessageStub::StateE::WriteBodyContinue;

    if (rv >= 0) {

        if (rmsgstub.isRelay()) {
            _rpacket_options.request_accept = true;
        }

        if (rmsgstub.serializer_ptr_->empty()) {
            //we've just finished serializing body
            cmd |= static_cast<uint8_t>(PacketHeader::CommandE::EndMessageFlag);
            solid_log(logger, Info, this << " stored message body with index = " << _msgidx << " and size = " << rv << " cmd = " << (int)cmd << " is_relayed = " << rmsgstub.isRelay());
            doTryCompleteMessageAfterSerialization(_msgidx, _rsender, _rerror);
        } else {
            ++rmsgstub.packet_count_;
        }

        _rsender.protocol().storeValue(pcmdpos, cmd);

        //store the data size
        _rsender.protocol().storeValue(psizepos, static_cast<uint16_t>(rv));

        _pbufpos += rv;
    } else {
        _rerror = rmsgstub.serializer_ptr_->error();
    }

    return _pbufpos;
}
//-----------------------------------------------------------------------------
char* MessageWriter::doWriteRelayedHead(
    char*        _pbufpos,
    char*        _pbufend,
    const size_t _msgidx,
    PacketOptions& /*_rpacket_options*/,
    Sender&                      _rsender,
    const PacketHeader::CommandE _cmd,
    ErrorConditionT&             _rerror)
{
    uint8_t cmd = static_cast<uint8_t>(_cmd);
    _pbufpos    = _rsender.protocol().storeValue(_pbufpos, cmd);

    _pbufpos = _rsender.protocol().storeCrossValue(_pbufpos, _pbufend - _pbufpos, static_cast<uint32_t>(_msgidx));
    solid_check_log(_pbufpos != nullptr, logger, "fail store cross value");

    char* psizepos = _pbufpos;
    _pbufpos       = _rsender.protocol().storeValue(psizepos, static_cast<uint16_t>(0));

    MessageStub& rmsgstub = message_vec_[_msgidx];

    _rsender.context().request_id.index  = static_cast<uint32_t>(_msgidx + 1);
    _rsender.context().request_id.unique = rmsgstub.unique_;
    _rsender.context().message_flags     = rmsgstub.prelay_data_->pmessage_header_->flags_;
    _rsender.context().message_flags.set(MessageFlagsE::Relayed);
    _rsender.context().pmessage_url = &rmsgstub.prelay_data_->pmessage_header_->url_;

    const ptrdiff_t rv = rmsgstub.state_ == MessageStub::StateE::RelayedHeadStart ? rmsgstub.serializer_ptr_->run(_rsender.context(), _pbufpos, _pbufend - _pbufpos, *rmsgstub.prelay_data_->pmessage_header_) : rmsgstub.serializer_ptr_->run(_rsender.context(), _pbufpos, _pbufend - _pbufpos);
    rmsgstub.state_    = MessageStub::StateE::RelayedHeadContinue;

    if (rv >= 0) {
        _rsender.protocol().storeValue(psizepos, static_cast<uint16_t>(rv));
        solid_log(logger, Verbose, "stored message header with index = " << _msgidx << " and size = " << rv);

        if (rmsgstub.serializer_ptr_->empty()) {
            //we've just finished serializing header
            cache(rmsgstub.serializer_ptr_);
            rmsgstub.state_ = MessageStub::StateE::RelayedBody;
        }
        _pbufpos += rv;
    } else {
        cache(rmsgstub.serializer_ptr_);
        _rerror = rmsgstub.serializer_ptr_->error();
    }

    return _pbufpos;
}
//-----------------------------------------------------------------------------
char* MessageWriter::doWriteRelayedBody(
    char*        _pbufpos,
    char*        _pbufend,
    const size_t _msgidx,
    PacketOptions& /*_rpacket_options*/,
    Sender& _rsender,
    ErrorConditionT& /*_rerror*/)
{
    char* pcmdpos = nullptr;

    pcmdpos = _pbufpos;
    _pbufpos += 1;

    _pbufpos = _rsender.protocol().storeCrossValue(_pbufpos, _pbufend - _pbufpos, static_cast<uint32_t>(_msgidx));
    solid_check_log(_pbufpos != nullptr, logger, "fail store cross value");

    char* psizepos = _pbufpos;
    _pbufpos       = _rsender.protocol().storeValue(psizepos, static_cast<uint16_t>(0));
    uint8_t cmd    = static_cast<uint8_t>(PacketHeader::CommandE::Message);

    MessageStub& rmsgstub = message_vec_[_msgidx];

    solid_assert_log(rmsgstub.prelay_data_, logger);

    size_t towrite = _pbufend - _pbufpos;
    if (towrite > rmsgstub.relay_size_) {
        towrite = rmsgstub.relay_size_;
    }

    memcpy(_pbufpos, rmsgstub.prelay_pos_, towrite);

    _pbufpos += towrite;
    rmsgstub.prelay_pos_ += towrite;
    rmsgstub.relay_size_ -= towrite;

    solid_log(logger, Verbose, "storing " << towrite << " bytes"
                                          << " for msg " << _msgidx << " cmd = " << (int)cmd << " flags = " << rmsgstub.prelay_data_->flags_ << " relaydata = " << rmsgstub.prelay_data_);

    if (rmsgstub.relay_size_ == 0) {
        solid_assert_log(write_inner_list_.size(), logger);
        doWriteQueueErase(_msgidx, __LINE__);

        const bool is_message_end  = rmsgstub.prelay_data_->isMessageEnd();
        const bool is_message_last = rmsgstub.prelay_data_->isMessageLast();
        const bool is_request      = rmsgstub.prelay_data_->isRequest(); //Message::is_waiting_response(rmsgstub.prelay_data_->pmessage_header_->flags_);

        solid_log(logger, Verbose, this << " completeRelayed " << _msgidx << " is_end = " << is_message_end << " is_last " << is_message_last << " is_req = " << is_request);
        _rsender.completeRelayed(rmsgstub.prelay_data_, rmsgstub.pool_msg_id_);
        rmsgstub.prelay_data_ = nullptr; //when prelay_data_ is null we consider the message not in write_inner_list_

        if (is_message_end) {
            cmd |= static_cast<uint8_t>(PacketHeader::CommandE::EndMessageFlag);
        }

        if (is_message_last) {
            if (is_request) {
                rmsgstub.state_ = MessageStub::StateE::RelayedWait;
            } else {
                order_inner_list_.erase(_msgidx);
                doUnprepareMessageStub(_msgidx);
            }
        }
    }

    _rsender.protocol().storeValue(pcmdpos, cmd);

    //store the data size
    _rsender.protocol().storeValue(psizepos, static_cast<uint16_t>(towrite));
    return _pbufpos;
}
//-----------------------------------------------------------------------------
char* MessageWriter::doWriteMessageCancel(
    char*        _pbufpos,
    char*        _pbufend,
    const size_t _msgidx,
    PacketOptions& /*_rpacket_options*/,
    Sender& _rsender,
    ErrorConditionT& /*_rerror*/)
{

    solid_log(logger, Verbose, "" << _msgidx);

    uint8_t cmd = static_cast<uint8_t>(PacketHeader::CommandE::CancelMessage);
    _pbufpos    = _rsender.protocol().storeValue(_pbufpos, cmd);

    _pbufpos = _rsender.protocol().storeCrossValue(_pbufpos, _pbufend - _pbufpos, static_cast<uint32_t>(_msgidx));
    solid_check_log(_pbufpos != nullptr, logger, "fail store cross value");

    solid_assert_log(write_inner_list_.size(), logger);
    doWriteQueueErase(_msgidx, __LINE__);
    order_inner_list_.erase(_msgidx);
    doUnprepareMessageStub(_msgidx);
    if (write_queue_sync_index_ == _msgidx) {
        write_queue_sync_index_ = InvalidIndex();
    }
    return _pbufpos;
}
//-----------------------------------------------------------------------------
char* MessageWriter::doWriteRelayedCancel(
    char*        _pbufpos,
    char*        _pbufend,
    const size_t _msgidx,
    PacketOptions& /*_rpacket_options*/,
    Sender& _rsender,
    ErrorConditionT& /*_rerror*/)
{
    MessageStub& rmsgstub = message_vec_[_msgidx];

    solid_log(logger, Error, "" << _msgidx << " uid = " << rmsgstub.unique_ << " state = " << (int)rmsgstub.state_);

    uint8_t cmd = static_cast<uint8_t>(PacketHeader::CommandE::CancelMessage);
    _pbufpos    = _rsender.protocol().storeValue(_pbufpos, cmd);

    _pbufpos = _rsender.protocol().storeCrossValue(_pbufpos, _pbufend - _pbufpos, static_cast<uint32_t>(_msgidx));

    //TODO: add support for sending also rmsgstub.prelay_data_->pmessage_header_->recipient_request_id_
    solid_check_log(_pbufpos != nullptr, logger, "fail store cross value");
    solid_assert_log(rmsgstub.prelay_data_ == nullptr, logger);
    //_rsender.completeRelayed(rmsgstub.prelay_data_, rmsgstub.pool_msg_id_);
    //rmsgstub.prelay_data_ = nullptr;msgstub.prelay_data_

    solid_assert_log(write_inner_list_.size(), logger);
    doWriteQueueErase(_msgidx, __LINE__);
    order_inner_list_.erase(_msgidx);
    doUnprepareMessageStub(_msgidx);
    if (write_queue_sync_index_ == _msgidx) {
        write_queue_sync_index_ = InvalidIndex();
    }
    return _pbufpos;
}
//-----------------------------------------------------------------------------
char* MessageWriter::doWriteRelayedCancelRequest(
    char*        _pbufpos,
    char*        _pbufend,
    const size_t _msgidx,
    PacketOptions& /*_rpacket_options*/,
    Sender& _rsender,
    ErrorConditionT& /*_rerror*/)
{

    solid_log(logger, Verbose, "" << _msgidx);

    uint8_t cmd = static_cast<uint8_t>(PacketHeader::CommandE::CancelMessage);
    _pbufpos    = _rsender.protocol().storeValue(_pbufpos, cmd);

    _pbufpos = _rsender.protocol().storeCrossValue(_pbufpos, _pbufend - _pbufpos, static_cast<uint32_t>(_msgidx));
    solid_check_log(_pbufpos != nullptr, logger, "fail store cross value");

    solid_assert_log(write_inner_list_.size(), logger);
    doWriteQueueErase(_msgidx, __LINE__);
    order_inner_list_.erase(_msgidx);
    doUnprepareMessageStub(_msgidx);
    if (write_queue_sync_index_ == _msgidx) {
        write_queue_sync_index_ = InvalidIndex();
    }
    return _pbufpos;
}
//-----------------------------------------------------------------------------
void MessageWriter::doTryCompleteMessageAfterSerialization(
    const size_t     _msgidx,
    Sender&          _rsender,
    ErrorConditionT& _rerror)
{

    MessageStub& rmsgstub(message_vec_[_msgidx]);
    RequestId    requid(static_cast<uint32_t>(_msgidx), rmsgstub.unique_);

    solid_log(logger, Info, this << " done serializing message " << requid << ". Message id sent to client " << _rsender.context().request_id);

    cache(rmsgstub.serializer_ptr_);

    solid_assert_log(write_inner_list_.size(), logger);
    doWriteQueueErase(_msgidx, __LINE__);

    rmsgstub.msgbundle_.message_flags.reset(MessageFlagsE::StartedSend);
    rmsgstub.msgbundle_.message_flags.set(MessageFlagsE::DoneSend);

    rmsgstub.serializer_ptr_ = nullptr;
    rmsgstub.state_          = MessageStub::StateE::WriteStart;

    solid_log(logger, Verbose, MessageWriterPrintPairT(*this, PrintInnerListsE));

    if (!Message::is_awaiting_response(rmsgstub.msgbundle_.message_flags)) {
        //no wait response for the message - complete
        MessageBundle tmp_msg_bundle(std::move(rmsgstub.msgbundle_));
        MessageId     tmp_pool_msg_id(rmsgstub.pool_msg_id_);

        order_inner_list_.erase(_msgidx);
        doUnprepareMessageStub(_msgidx);

        _rerror = _rsender.completeMessage(tmp_msg_bundle, tmp_pool_msg_id);

        solid_log(logger, Verbose, MessageWriterPrintPairT(*this, PrintInnerListsE));
    } else {
        rmsgstub.state_ = MessageStub::StateE::WriteWait;
    }

    solid_log(logger, Verbose, MessageWriterPrintPairT(*this, PrintInnerListsE));
}

//-----------------------------------------------------------------------------
void MessageWriter::forEveryMessagesNewerToOlder(VisitFunctionT const& _rvisit_fnc)
{

    solid_log(logger, Verbose, "");
    size_t msgidx = order_inner_list_.backIndex();

    while (msgidx != InvalidIndex()) {
        MessageStub& rmsgstub = message_vec_[msgidx];

        const bool message_in_write_queue = !rmsgstub.isWaitResponseState();

        if (rmsgstub.msgbundle_.message_ptr) { //skip relayed messages
            _rvisit_fnc(
                rmsgstub.msgbundle_,
                rmsgstub.pool_msg_id_);

            if (!rmsgstub.msgbundle_.message_ptr) { //message fetched

                if (message_in_write_queue) {
                    solid_assert_log(write_inner_list_.size(), logger);
                    doWriteQueueErase(msgidx, __LINE__);
                }

                const size_t oldidx = msgidx;

                msgidx = order_inner_list_.previousIndex(oldidx);

                order_inner_list_.erase(oldidx);
                doUnprepareMessageStub(oldidx);

            } else {

                msgidx = order_inner_list_.previousIndex(msgidx);
            }
        }
    } //while
}

//-----------------------------------------------------------------------------

void MessageWriter::print(std::ostream& _ros, const PrintWhat _what) const
{
    if (_what == PrintInnerListsE) {
        _ros << "InnerLists: ";
        auto print_fnc = [&_ros](const size_t _idx, MessageStub const&) { _ros << _idx << ' '; };
        _ros << "OrderList: ";
        order_inner_list_.forEach(print_fnc);
        _ros << '\t';

        _ros << "WriteList: ";
        write_inner_list_.forEach(print_fnc);
        _ros << '\t';

        _ros << "CacheList size: " << cache_inner_list_.size();
        //cache_inner_list_.forEach(print_fnc);
        _ros << '\t';
    }
}

//-----------------------------------------------------------------------------

void MessageWriter::cache(Serializer::PointerT& _ser)
{
    if (_ser) {
        _ser->clear();
        _ser->link(serializer_stack_top_);
        serializer_stack_top_ = std::move(_ser);
    }
}

//-----------------------------------------------------------------------------

Serializer::PointerT MessageWriter::createSerializer(Sender& _sender)
{
    if (serializer_stack_top_) {
        Serializer::PointerT ser{std::move(serializer_stack_top_)};
        serializer_stack_top_ = std::move(ser->link());
        _sender.protocol().reconfigure(*ser, _sender.configuration());
        return ser;
    }
    return _sender.protocol().createSerializer(_sender.configuration());
}

//-----------------------------------------------------------------------------

/*virtual*/ MessageWriter::Sender::~Sender()
{
}
/*virtual*/ ErrorConditionT MessageWriter::Sender::completeMessage(MessageBundle& /*_rmsgbundle*/, MessageId const& /*_rmsgid*/)
{
    return ErrorConditionT{};
}
/*virtual*/ void MessageWriter::Sender::completeRelayed(RelayData* /*_relay_data*/, MessageId const& /*_rmsgid*/)
{
}
/*virtual*/ bool MessageWriter::Sender::cancelMessage(MessageBundle& /*_rmsgbundle*/, MessageId const& /*_rmsgid*/)
{
    return true;
}
/*virtual*/ void MessageWriter::Sender::cancelRelayed(RelayData* /*_relay_data*/, MessageId const& /*_rmsgid*/)
{
}
//-----------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& _ros, std::pair<MessageWriter const&, MessageWriter::PrintWhat> const& _msgwriterpair)
{
    _msgwriterpair.first.print(_ros, _msgwriterpair.second);
    return _ros;
}
//-----------------------------------------------------------------------------
} //namespace mprpc
} //namespace frame
} //namespace solid
