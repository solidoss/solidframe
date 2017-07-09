// solid/frame/ipc/src/ipcmessagereader.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "mpipcmessagewriter.hpp"
#include "mpipcutility.hpp"

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcerror.hpp"

#include "solid/system/cassert.hpp"
#include "solid/system/debug.hpp"

namespace solid {
namespace frame {
namespace mpipc {
//-----------------------------------------------------------------------------
MessageWriter::MessageWriter()
    : current_message_type_id_(InvalidIndex())
    , current_synchronous_message_idx_(InvalidIndex())
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
bool MessageWriter::enqueue(
    WriterConfiguration const& _rconfig,
    MessageBundle&             _rmsgbundle,
    MessageId const&           _rpool_msg_id,
    MessageId&                 _rconn_msg_id)
{

    //see if we can accept the message
    if (full(_rconfig) or cache_inner_list_.empty()) {
        return false;
    }

    //see if we have too many messages waiting for responses
    if (
        Message::is_waiting_response(_rmsgbundle.message_flags) and ((order_inner_list_.size() - write_inner_list_.size()) >= _rconfig.max_message_count_response_wait)) {
        return false;
    }

    SOLID_ASSERT(_rmsgbundle.message_ptr.get());

    //clear all disrupting flags
    _rmsgbundle.message_flags.reset(MessageFlagsE::StartedSend).reset(MessageFlagsE::DoneSend);

    const size_t idx = cache_inner_list_.popFront();
    MessageStub& rmsgstub(message_vec_[idx]);

    rmsgstub.msgbundle_   = std::move(_rmsgbundle);
    rmsgstub.pool_msg_id_ = _rpool_msg_id;

    _rconn_msg_id = MessageId(idx, rmsgstub.unique_);

    order_inner_list_.pushBack(idx);
    write_inner_list_.pushBack(idx);
    vdbgx(Debug::mpipc, MessageWriterPrintPairT(*this, PrintInnerListsE));

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
bool MessageWriter::cancel(
    MessageId const& _rmsguid,
    MessageBundle&   _rmsgbundle,
    MessageId&       _rpool_msg_id)
{
    if (_rmsguid.isValid() and _rmsguid.index < message_vec_.size() and _rmsguid.unique == message_vec_[_rmsguid.index].unique_) {
        return doCancel(_rmsguid.index, _rmsgbundle, _rpool_msg_id);
    }

    return false;
}
//-----------------------------------------------------------------------------
MessagePointerT MessageWriter::fetchRequest(MessageId const& _rmsguid) const
{
    if (_rmsguid.isValid() and _rmsguid.index < message_vec_.size() and _rmsguid.unique == message_vec_[_rmsguid.index].unique_) {
        const MessageStub& rmsgstub = message_vec_[_rmsguid.index];
        return MessagePointerT(rmsgstub.msgbundle_.message_ptr);
    }
    return MessagePointerT();
}
//-----------------------------------------------------------------------------
bool MessageWriter::cancelOldest(
    MessageBundle& _rmsgbundle,
    MessageId&     _rpool_msg_id)
{
    if (order_inner_list_.size()) {
        return doCancel(order_inner_list_.frontIndex(), _rmsgbundle, _rpool_msg_id);
    }
    return false;
}
//-----------------------------------------------------------------------------
bool MessageWriter::doCancel(
    const size_t   _msgidx,
    MessageBundle& _rmsgbundle,
    MessageId&     _rpool_msg_id)
{

    vdbgx(Debug::mpipc, "");

    MessageStub& rmsgstub = message_vec_[_msgidx];

    if (Message::is_canceled(rmsgstub.msgbundle_.message_flags)) {
        return false; //already canceled
    }

    rmsgstub.cancel();

    _rmsgbundle   = std::move(rmsgstub.msgbundle_);
    _rpool_msg_id = rmsgstub.pool_msg_id_;

    order_inner_list_.erase(_msgidx);

    if (rmsgstub.serializer_ptr_.get()) {
        //the message is being sent
        rmsgstub.serializer_ptr_->clear();
    } else if (Message::is_waiting_response(rmsgstub.msgbundle_.message_flags)) {
        //message is waiting response
        doUnprepareMessageStub(_msgidx);
    } else {
        //message is waiting to be sent
        write_inner_list_.erase(_msgidx);
        doUnprepareMessageStub(_msgidx);
    }

    return true;
}
//-----------------------------------------------------------------------------
bool MessageWriter::empty() const
{
    return order_inner_list_.empty();
}
//-----------------------------------------------------------------------------

bool MessageWriter::isFrontRelayMessage() const
{
    return not write_inner_list_.empty() and write_inner_list_.front().isRelay();
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

uint32_t MessageWriter::write(
    char*                      _pbuf,
    uint32_t                   _bufsz,
    const WriteFlagsT&         _flags,
    uint8_t                    _ackd_buf_count,
    CompleteFunctionT&         _complete_fnc,
    WriterConfiguration const& _rconfig,
    Protocol const&            _rproto,
    ConnectionContext& _rctx, ErrorConditionT& _rerror)
{

    char* pbufpos = _pbuf;
    char* pbufend = _pbuf + _bufsz;

    uint32_t freesz = pbufend - pbufpos;

    bool more = true;

    while (freesz >= (PacketHeader::SizeOfE + _rproto.minimumFreePacketDataSize()) and more) {

        PacketHeader  packet_header(PacketHeader::MessageTypeE, 0, 0);
        PacketOptions packet_options;
        char*         pbufdata = pbufpos + PacketHeader::SizeOfE;
        char*         pbuftmp  = doFillPacket(pbufdata, pbufend, packet_options, more, _flags, _ackd_buf_count, _complete_fnc, _rconfig, _rproto, _rctx, _rerror);

        if (pbuftmp != pbufdata) {

            if (not packet_options.force_no_compress) {
                ErrorConditionT compress_error;
                size_t          compressed_size = _rconfig.inplace_compress_fnc(pbufdata, pbuftmp - pbufdata, compress_error);

                if (compressed_size) {
                    packet_header.flags(packet_header.flags() | PacketHeader::CompressedFlagE);
                    pbuftmp = pbufdata + compressed_size;
                } else if (!compress_error) {
                    //the buffer was not modified, we can send it uncompressed
                } else {
                    //there was an error and the inplace buffer was changed - exit with error
                    more    = false;
                    _rerror = compress_error;
                    continue;
                }
            }

            SOLID_ASSERT(static_cast<size_t>(pbuftmp - pbufdata) < static_cast<size_t>(0xffff));

            packet_header.type(packet_options.packet_type);
            packet_header.size(pbuftmp - pbufdata);

            if (_ackd_buf_count) {
                packet_header.flags(packet_header.flags() | PacketHeader::AckCountFlagE);
                _ackd_buf_count = 0;
            }

            pbufpos = packet_header.store(pbufpos, _rproto);
            pbufpos = pbuftmp;
            freesz  = pbufend - pbufpos;
        } else {
            more = false;
        }
    }

    if (not _rerror and pbufpos == _pbuf) {
        if (_flags.has(WriteFlagsE::ShouldSendKeepAlive)) {
            PacketHeader packet_header(PacketHeader::KeepAliveTypeE, PacketHeader::AckCountFlagE, sizeof(_ackd_buf_count));
            pbufpos = packet_header.store(pbufpos, _rproto);
            pbufpos = _rproto.storeValue(pbufpos, _ackd_buf_count);
        } else if (_ackd_buf_count) {
            PacketHeader packet_header(PacketHeader::UpdateTypeE, PacketHeader::AckCountFlagE, sizeof(_ackd_buf_count));
            pbufpos = packet_header.store(pbufpos, _rproto);
            pbufpos = _rproto.storeValue(pbufpos, _ackd_buf_count);
        }
    }
    return pbufpos - _pbuf;
}
//-----------------------------------------------------------------------------
//  |4B - PacketHeader|PacketHeader.size - PacketData|
//  Examples:
//
//  3 Messages Packet
//  |[PH(NewMessageTypeE)]|[MessageData-1][1B - DataType = NewMessageTypeE][MessageData-2][NewMessageTypeE][MessageData-3]|
//
//  2 Messages, one spread over 3 packets and one new:
//  |[PH(NewMessageTypeE)]|[MessageData-1]|
//  |[PH(ContinuedMessageTypeE)]|[MessageData-1]|
//  |[PH(ContinuedMessageTypeE)]|[MessageData-1][NewMessageTypeE][MessageData-2]|
//
//  3 Messages, one Continued, one old continued and one new
//  |[PH(ContinuedMessageTypeE)]|[MessageData-3]|
//  |[PH(ContinuedMessageTypeE)]|[MessageData-3]| # message 3 not finished
//  |[PH(OldMessageTypeE)]|[MessageData-2]|
//  |[PH(ContinuedMessageTypeE)]|[MessageData-2]|
//  |[PH(ContinuedMessageTypeE)]|[MessageData-2]|
//  |[PH(ContinuedMessageTypeE)]|[MessageData-2][NewMessageTypeE][MessageData-4][OldMessageTypeE][MessageData-3]| # message 2 finished, message 3 still not finished
//  |[PH(ContinuedMessageTypeE)]|[MessageData-3]|
//
//
//  NOTE:
//  Header type can be: NewMessageTypeE, OldMessageTypeE, ContinuedMessageTypeE
//  Data type can be: NewMessageTypeE, OldMessageTypeE
//
//  PROBLEM:
//  1) Should we call prepare on a message, and if so when?
//      > If we call it on doFillPacket, we cannot use prepare as a flags filter.
//          This is because of Send Synchronous Flag which, if sent on prepare would be too late.
//          One cannot set Send Synchronous Flag because the connection might not be the
//          one handling Synchronous Messages.
//
//      > If we call it before message gets to a connection we do not have a MessageId (e.g. can be used for tracing).
//
//
//      * Decided to drop the "prepare" functionality.
//
char* MessageWriter::doFillPacket(
    char*                      _pbufbeg,
    char*                      _pbufend,
    PacketOptions&             _rpacket_options,
    bool&                      _rmore,
    const WriteFlagsT&         _flags,
    uint8_t                    _ackd_buf_count,
    CompleteFunctionT&         _complete_fnc,
    WriterConfiguration const& _rconfig,
    Protocol const&            _rproto,
    ConnectionContext&         _rctx,
    ErrorConditionT&           _rerror)
{

    char*              pbufpos = _pbufbeg;
    uint32_t           freesz  = _pbufend - pbufpos;
    SerializerPointerT tmp_serializer;
    size_t             packet_message_count = 0;
    size_t             loop_guard           = write_inner_list_.size();

    while (write_inner_list_.size() and freesz >= _rproto.minimumFreePacketDataSize() and (loop_guard--) != 0) {
        if (not _flags.has(WriteFlagsE::CanSendRelayedMessages) and write_inner_list_.front().isRelay()) {
            write_inner_list_.pushBack(write_inner_list_.popFront());
            continue;
        }

        const size_t        msgidx    = write_inner_list_.frontIndex();
        MessageStub&        rmsgstub  = message_vec_[msgidx];
        PacketHeader::Types msgswitch = PacketHeader::MessageTypeE;

        doPrepareMessageForSending(msgidx, _rconfig, _rproto, _rctx, tmp_serializer);

        char* pswitchpos = nullptr;

        if (packet_message_count) {
            pswitchpos = pbufpos;
            pbufpos += sizeof(char); //skip one byte - the switch type
        }

        pbufpos = _rproto.storeCrossValue(pbufpos, _pbufend - pbufpos, static_cast<uint32_t>(msgidx));
        SOLID_CHECK(pbufpos != nullptr, "fail store cross value");

        ++packet_message_count;

        if (rmsgstub.isCanceled()) {
            //message already completed - just drop it from write list
            write_inner_list_.erase(msgidx);
            doUnprepareMessageStub(msgidx);
            if (current_synchronous_message_idx_ == msgidx) {
                current_synchronous_message_idx_ = InvalidIndex();
            }

            if (pswitchpos) {
                uint8_t tmp = static_cast<uint8_t>(msgswitch);
                _rproto.storeValue(pswitchpos, tmp);
            } else {
                //first message in the packet
                _rpacket_options.packet_type = msgswitch;
            }
            continue;
        }

        _rctx.request_id.index  = (msgidx + 1);
        _rctx.request_id.unique = rmsgstub.unique_;

        _rctx.message_flags = Message::clear_state_flags(rmsgstub.msgbundle_.message_flags) | Message::state_flags(rmsgstub.msgbundle_.message_ptr->flags());
        _rctx.message_flags = Message::update_state_flags(_rctx.message_flags);

        _rctx.pmessage_url = &rmsgstub.msgbundle_.message_url;

        if (_ackd_buf_count) {
            pbufpos         = _rproto.storeValue(pbufpos, _ackd_buf_count);
            _ackd_buf_count = 0;
        }

        char* psizepos = pbufpos;
        pbufpos        = _rproto.storeValue(pbufpos, static_cast<uint16_t>(0));

        const int rv = rmsgstub.serializer_ptr_->run(_rctx, pbufpos, _pbufend - pbufpos);

        if (rv >= 0) {

            _rproto.storeValue(psizepos, static_cast<uint16_t>(rv));

            pbufpos += rv;
            freesz -= rv;

            if (rmsgstub.serializer_ptr_->empty()) {
                msgswitch = PacketHeader::EndMessageTypeE;
            }

            if (rmsgstub.state_ == MessageStub::StateE::WriteBody) {
                doTryCompleteMessageAfterSerialization(msgidx, _complete_fnc, _rconfig, _rproto, _rctx, tmp_serializer, _rerror);
            }

            //set the message switch type
            if (pswitchpos) {
                uint8_t tmp = static_cast<uint8_t>(msgswitch);
                _rproto.storeValue(pswitchpos, tmp);
            } else {
                //first message in the packet
                _rpacket_options.packet_type = msgswitch;
            }

            if (_rerror) {
                //pbufpos = nullptr;
                break;
            }

        } else {
            _rerror = rmsgstub.serializer_ptr_->error();
            pbufpos = nullptr;
            break;
        }
    }

    vdbgx(Debug::mpipc, "write_q_size " << write_inner_list_.size() << " order_q_size " << order_inner_list_.size());

    return pbufpos;
}
//-----------------------------------------------------------------------------
//Explanation:
//We can have multiple messages in the writeq. Some Synchronous and some asynchronous.
//The asynchronous messages can be multiplexed with the current synchronous one.
inline void MessageWriter::doLocateNextWriteMessage()
{
    if (current_synchronous_message_idx_ == InvalidIndex()) {
        return;
    } else if (not write_inner_list_.empty()) {
        //locate next asynchronous message or the current synchronous message
        while (current_synchronous_message_idx_ != write_inner_list_.frontIndex() and write_inner_list_.front().isSynchronous()) {
            write_inner_list_.pushBack(write_inner_list_.popFront());
        }
    }
}
//-----------------------------------------------------------------------------
void MessageWriter::doTryCompleteMessageAfterSerialization(
    const size_t               _msgidx,
    CompleteFunctionT&         _complete_fnc,
    WriterConfiguration const& _rconfig,
    Protocol const&            _rproto,
    ConnectionContext&         _rctx,
    SerializerPointerT&        _rtmp_serializer,
    ErrorConditionT&           _rerror)
{

    MessageStub& rmsgstub(message_vec_[_msgidx]);

    if (rmsgstub.serializer_ptr_->empty()) {
        RequestId requid(_msgidx, rmsgstub.unique_);

        vdbgx(Debug::mpipc, "done serializing message " << requid << ". Message id sent to client " << _rctx.request_id);

        _rtmp_serializer = std::move(rmsgstub.serializer_ptr_);
        //done serializing the message:

        write_inner_list_.popFront();

        if (current_synchronous_message_idx_ == _msgidx) {
            current_synchronous_message_idx_ = InvalidIndex();
        }

        doLocateNextWriteMessage();

        rmsgstub.msgbundle_.message_flags.reset(MessageFlagsE::StartedSend);
        rmsgstub.msgbundle_.message_flags.set(MessageFlagsE::DoneSend);

        rmsgstub.serializer_ptr_ = nullptr; //free some memory
        rmsgstub.state_          = MessageStub::StateE::NotStarted;

        vdbgx(Debug::mpipc, MessageWriterPrintPairT(*this, PrintInnerListsE));

        if (not Message::is_waiting_response(rmsgstub.msgbundle_.message_flags)) {
            //no wait response for the message - complete
            MessageBundle tmp_msg_bundle(std::move(rmsgstub.msgbundle_));
            MessageId     tmp_pool_msg_id(rmsgstub.pool_msg_id_);

            order_inner_list_.erase(_msgidx);
            doUnprepareMessageStub(_msgidx);

            _rerror = _complete_fnc(tmp_msg_bundle, tmp_pool_msg_id);

            vdbgx(Debug::mpipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
        } else {
            rmsgstub.msgbundle_.message_flags.set(MessageFlagsE::WaitResponse);
        }

        vdbgx(Debug::mpipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
    } else {
        //message not done - packet should be full
        ++rmsgstub.packet_count_;

        if (rmsgstub.packet_count_ >= _rconfig.max_message_continuous_packet_count) {
            rmsgstub.packet_count_ = 0;
            write_inner_list_.popFront();
            write_inner_list_.pushBack(_msgidx);

            doLocateNextWriteMessage();

            vdbgx(Debug::mpipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
        }
    }
}

//-----------------------------------------------------------------------------

void MessageWriter::doPrepareMessageForSending(
    const size_t               _msgidx,
    WriterConfiguration const& _rconfig,
    Protocol const&            _rproto,
    ConnectionContext&         _rctx,
    SerializerPointerT&        _rtmp_serializer)
{

    MessageStub& rmsgstub(message_vec_[_msgidx]);

    switch (rmsgstub.state_) {
    case MessageStub::StateE::NotStarted:
        //switch to new message
        if (_rtmp_serializer) {
            rmsgstub.serializer_ptr_ = std::move(_rtmp_serializer);
        } else {
            rmsgstub.serializer_ptr_ = _rproto.createSerializer();
        }

        _rproto.reset(*rmsgstub.serializer_ptr_);

        rmsgstub.msgbundle_.message_flags.set(MessageFlagsE::StartedSend);

        rmsgstub.state_ = MessageStub::StateE::WriteHead;

        vdbgx(Debug::mpipc, "message header url: " << rmsgstub.msgbundle_.message_url);

        rmsgstub.serializer_ptr_->push(rmsgstub.msgbundle_.message_ptr->header_);
        break;
    case MessageStub::StateE::Canceled:
        break;
    case MessageStub::StateE::WriteHead:
        if (rmsgstub.serializer_ptr_->empty()) {
            //we've just finished serializing header

            rmsgstub.serializer_ptr_->push(rmsgstub.msgbundle_.message_ptr, rmsgstub.msgbundle_.message_type_id);

            rmsgstub.state_ = MessageStub::StateE::WriteBody;
        }
        break;
    case MessageStub::StateE::WriteBody:
        break;
    case MessageStub::StateE::RelayBody:
        break;
    } //switch
}

//-----------------------------------------------------------------------------

void MessageWriter::forEveryMessagesNewerToOlder(VisitFunctionT const& _rvisit_fnc)
{

    { //iterate through non completed messages
        size_t msgidx = order_inner_list_.frontIndex();

        while (msgidx != InvalidIndex()) {
            MessageStub& rmsgstub = message_vec_[msgidx];

            if (not rmsgstub.msgbundle_.message_ptr) {
                SOLID_THROW("invalid message - something went wrong with the nested queue for message: " << msgidx);
                continue;
            }

            const bool message_in_write_queue = not Message::is_waiting_response(rmsgstub.msgbundle_.message_flags);

            _rvisit_fnc(
                rmsgstub.msgbundle_,
                rmsgstub.pool_msg_id_);

            if (not rmsgstub.msgbundle_.message_ptr) {

                if (message_in_write_queue) {
                    write_inner_list_.erase(msgidx);
                }

                const size_t oldidx = msgidx;

                msgidx = order_inner_list_.previousIndex(oldidx);

                order_inner_list_.erase(oldidx);
                doUnprepareMessageStub(oldidx);

            } else {

                msgidx = order_inner_list_.previousIndex(msgidx);
            }
        }
    }
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

std::ostream& operator<<(std::ostream& _ros, std::pair<MessageWriter const&, MessageWriter::PrintWhat> const& _msgwriterpair)
{
    _msgwriterpair.first.print(_ros, _msgwriterpair.second);
    return _ros;
}
//-----------------------------------------------------------------------------
} //namespace mpipc
} //namespace frame
} //namespace solid
