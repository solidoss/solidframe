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
    : current_message_type_id(InvalidIndex())
    , current_synchronous_message_idx(InvalidIndex())
    , order_inner_list(message_vec)
    , write_inner_list(message_vec)
    , cache_inner_list(message_vec)
{
}
//-----------------------------------------------------------------------------
MessageWriter::~MessageWriter() {}
//-----------------------------------------------------------------------------
void MessageWriter::prepare(WriterConfiguration const& _rconfig)
{
}
//-----------------------------------------------------------------------------
void MessageWriter::unprepare()
{
}
//-----------------------------------------------------------------------------
bool MessageWriter::full(WriterConfiguration const& _rconfig) const
{
    return write_inner_list.size() >= _rconfig.max_message_count_multiplex;
}
//-----------------------------------------------------------------------------
bool MessageWriter::enqueue(
    WriterConfiguration const& _rconfig,
    MessageBundle&             _rmsgbundle,
    MessageId const&           _rpool_msg_id,
    MessageId&                 _rconn_msg_id)
{

    //see if we can accept the message
    if (write_inner_list.size() >= _rconfig.max_message_count_multiplex) {
        return false;
    }

    if (
        Message::is_waiting_response(_rmsgbundle.message_flags) and order_inner_list.size() >= (_rconfig.max_message_count_multiplex + _rconfig.max_message_count_response_wait)) {
        return false;
    }

    SOLID_ASSERT(_rmsgbundle.message_ptr.get());

    //clear all disrupting flags
    _rmsgbundle.message_flags &= ~(0 | MessageFlags::StartedSend);
    _rmsgbundle.message_flags &= ~(0 | MessageFlags::DoneSend);

    size_t idx;

    if (cache_inner_list.size()) {
        idx = cache_inner_list.popFront();
    } else {
        idx = message_vec.size();
        message_vec.push_back(MessageStub());
    }

    MessageStub& rmsgstub(message_vec[idx]);

    rmsgstub.msgbundle   = std::move(_rmsgbundle);
    rmsgstub.pool_msg_id = _rpool_msg_id;

    _rconn_msg_id = MessageId(idx, rmsgstub.unique);

    order_inner_list.pushBack(idx);
    write_inner_list.pushBack(idx);
    vdbgx(Debug::mpipc, MessageWriterPrintPairT(*this, PrintInnerListsE));

    return true;
}
//-----------------------------------------------------------------------------
void MessageWriter::doUnprepareMessageStub(const size_t _msgidx)
{
    MessageStub& rmsgstub(message_vec[_msgidx]);

    rmsgstub.clear();
    cache_inner_list.pushFront(_msgidx);
}
//-----------------------------------------------------------------------------
bool MessageWriter::cancel(
    MessageId const& _rmsguid,
    MessageBundle&   _rmsgbundle,
    MessageId&       _rpool_msg_id)
{
    if (_rmsguid.isValid() and _rmsguid.index < message_vec.size() and _rmsguid.unique == message_vec[_rmsguid.index].unique) {
        return doCancel(_rmsguid.index, _rmsgbundle, _rpool_msg_id);
    }

    return false;
}
//-----------------------------------------------------------------------------
MessagePointerT MessageWriter::fetchRequest(MessageId const& _rmsguid) const
{
    if (_rmsguid.isValid() and _rmsguid.index < message_vec.size() and _rmsguid.unique == message_vec[_rmsguid.index].unique) {
        const MessageStub& rmsgstub = message_vec[_rmsguid.index];
        return MessagePointerT(rmsgstub.msgbundle.message_ptr);
    }
    return MessagePointerT();
}
//-----------------------------------------------------------------------------
bool MessageWriter::cancelOldest(
    MessageBundle& _rmsgbundle,
    MessageId&     _rpool_msg_id)
{
    if (order_inner_list.size()) {
        return doCancel(order_inner_list.frontIndex(), _rmsgbundle, _rpool_msg_id);
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

    MessageStub& rmsgstub = message_vec[_msgidx];

    if (Message::is_canceled(rmsgstub.msgbundle.message_flags)) {
        return false; //already canceled
    }

    rmsgstub.msgbundle.message_flags |= MessageFlags::Canceled;

    _rmsgbundle   = std::move(rmsgstub.msgbundle);
    _rpool_msg_id = rmsgstub.pool_msg_id;

    order_inner_list.erase(_msgidx);

    if (rmsgstub.serializer_ptr.get()) {
        //the message is being sent
        rmsgstub.serializer_ptr->clear();
    } else if (Message::is_waiting_response(rmsgstub.msgbundle.message_flags)) {
        //message is waiting response
        doUnprepareMessageStub(_msgidx);
    } else {
        //message is waiting to be sent
        write_inner_list.erase(_msgidx);
        doUnprepareMessageStub(_msgidx);
    }

    return true;
}
//-----------------------------------------------------------------------------
bool MessageWriter::empty() const
{
    return order_inner_list.empty();
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
    char* _pbuf, uint32_t _bufsz, const bool _keep_alive,
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
        PacketHeader  packet_header(PacketHeader::SwitchToNewMessageTypeE, 0, 0);
        PacketOptions packet_options;
        char*         pbufdata = pbufpos + PacketHeader::SizeOfE;
        char*         pbuftmp  = doFillPacket(pbufdata, pbufend, packet_options, more, _complete_fnc, _rconfig, _rproto, _rctx, _rerror);

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

            pbufpos = packet_header.store(pbufpos, _rproto);
            pbufpos = pbuftmp;
            freesz  = pbufend - pbufpos;
        } else {
            more = false;
        }
    }

    if (not _rerror) {
        if (pbufpos == _pbuf and _keep_alive) {
            PacketHeader packet_header(PacketHeader::KeepAliveTypeE, 0, 0);
            pbufpos = packet_header.store(pbufpos, _rproto);
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
    CompleteFunctionT&         _complete_fnc,
    WriterConfiguration const& _rconfig,
    Protocol const&            _rproto,
    ConnectionContext&         _rctx,
    ErrorConditionT&           _rerror)
{

    char*    pbufpos = _pbufbeg;
    uint32_t freesz  = _pbufend - pbufpos;

    SerializerPointerT tmp_serializer;
    size_t             packet_message_count = 0;

    while (write_inner_list.size() and freesz >= _rproto.minimumFreePacketDataSize()) {

        const size_t msgidx = write_inner_list.frontIndex();

        MessageStub&        rmsgstub = message_vec[msgidx];
        PacketHeader::Types msgswitch; // = PacketHeader::ContinuedMessageTypeE;

        if (
            Message::is_synchronous(rmsgstub.msgbundle.message_flags)) {
            if (current_synchronous_message_idx == InvalidIndex() or current_synchronous_message_idx == msgidx) {
                current_synchronous_message_idx = msgidx;
            } else {
                write_inner_list.popFront();
                write_inner_list.pushBack(msgidx);
                continue;
            }
        }
        
        bool just_written_message_header;
        bool currently_writing_message_header;
        
        msgswitch = doPrepareMessageForSending(
            msgidx, _rconfig, _rproto, _rctx, tmp_serializer,
            just_written_message_header,
            currently_writing_message_header
        );

        if (packet_message_count == 0) {
            //first message in the packet
            _rpacket_options.packet_type = msgswitch;
        } else /*if(not just_written_message_header)*/{
            uint8_t tmp = static_cast<uint8_t>(msgswitch);
            pbufpos     = _rproto.storeValue(pbufpos, tmp);
        }

        ++packet_message_count;
        
        if (rmsgstub.isCanceled()) {
            //message already completed - just drop it from write list
            write_inner_list.erase(msgidx);
            doUnprepareMessageStub(msgidx);
            if (current_synchronous_message_idx == msgidx) {
                current_synchronous_message_idx = InvalidIndex();
            }
            continue;
        }

        _rctx.request_id.index  = (msgidx + 1);
        _rctx.request_id.unique = rmsgstub.unique;

        _rctx.message_flags = Message::clear_state_flags(rmsgstub.msgbundle.message_flags) | Message::state_flags(rmsgstub.msgbundle.message_ptr->flags());
        _rctx.message_flags = Message::update_state_flags(_rctx.message_flags);
        
        char *psizepos = pbufpos;
        
        if(not currently_writing_message_header){
            pbufpos = _rproto.storeValue(pbufpos, static_cast<uint16_t>(0));
        }
        
        const int rv = rmsgstub.serializer_ptr->run(_rctx, pbufpos, _pbufend - pbufpos);

        if (rv > 0) {

            pbufpos += rv;
            freesz -= rv;
            
            if(not currently_writing_message_header){
                _rproto.storeValue(psizepos, static_cast<uint16_t>(rv));
                doTryCompleteMessageAfterSerialization(msgidx, _complete_fnc, _rconfig, _rproto, _rctx, tmp_serializer, _rerror);
            }
            
            
            if (_rerror) {
                //pbufpos = nullptr;
                break;
            }

        } else {
            _rerror = rmsgstub.serializer_ptr->error();
            pbufpos = nullptr;
            break;
        }
    }

    vdbgx(Debug::mpipc, "write_q_size " << write_inner_list.size() << " order_q_size " << order_inner_list.size());

    return pbufpos;
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

    MessageStub& rmsgstub(message_vec[_msgidx]);

    if (rmsgstub.serializer_ptr->empty()) {
        RequestId requid(_msgidx, rmsgstub.unique);
        
        vdbgx(Debug::mpipc, "done serializing message " << requid << ". Message id sent to client " << _rctx.request_id);
        
        _rtmp_serializer = std::move(rmsgstub.serializer_ptr);
        //done serializing the message:

        write_inner_list.popFront();

        if (current_synchronous_message_idx == _msgidx) {
            current_synchronous_message_idx = InvalidIndex();
        }

        rmsgstub.msgbundle.message_flags &= (~(0 | MessageFlags::StartedSend));
        rmsgstub.msgbundle.message_flags |= MessageFlags::DoneSend;

        rmsgstub.serializer_ptr = nullptr; //free some memory

        vdbgx(Debug::mpipc, MessageWriterPrintPairT(*this, PrintInnerListsE));

        if (not Message::is_waiting_response(rmsgstub.msgbundle.message_flags)) {
            //no wait response for the message - complete
            MessageBundle tmp_msg_bundle(std::move(rmsgstub.msgbundle));
            MessageId     tmp_pool_msg_id(rmsgstub.pool_msg_id);

            order_inner_list.erase(_msgidx);
            doUnprepareMessageStub(_msgidx);

            _rerror = _complete_fnc(tmp_msg_bundle, tmp_pool_msg_id);

            vdbgx(Debug::mpipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
        } else {
            rmsgstub.msgbundle.message_flags |= MessageFlags::WaitResponse;
        }

        vdbgx(Debug::mpipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
    } else {
        //message not done - packet should be full
        ++rmsgstub.packet_count;

        if (rmsgstub.packet_count >= _rconfig.max_message_continuous_packet_count) {
            rmsgstub.packet_count = 0;
            write_inner_list.popFront();
            write_inner_list.pushBack(_msgidx);
            vdbgx(Debug::mpipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
        }
    }
}

//-----------------------------------------------------------------------------

PacketHeader::Types MessageWriter::doPrepareMessageForSending(
    const size_t               _msgidx,
    WriterConfiguration const& _rconfig,
    Protocol const&            _rproto,
    ConnectionContext&         _rctx,
    SerializerPointerT&        _rtmp_serializer,
    bool &_rjust_written_message_header,
    bool &_rcurrently_writing_message_header)
{

    PacketHeader::Types msgswitch; // = PacketHeader::ContinuedMessageTypeE;

    MessageStub& rmsgstub(message_vec[_msgidx]);
    
    _rjust_written_message_header = false;
    _rcurrently_writing_message_header = false;
    
    if (not rmsgstub.serializer_ptr) {

        //switch to new message
        msgswitch = PacketHeader::SwitchToNewMessageTypeE;

        if (_rtmp_serializer) {
            rmsgstub.serializer_ptr = std::move(_rtmp_serializer);
        } else {
            rmsgstub.serializer_ptr = _rproto.createSerializer();
        }

        _rproto.reset(*rmsgstub.serializer_ptr);

        rmsgstub.msgbundle.message_flags |= MessageFlags::StartedSend;
        rmsgstub.pmsgheader = &rmsgstub.msgbundle.message_ptr->header_;
        
        _rcurrently_writing_message_header = true;

        rmsgstub.serializer_ptr->push(*rmsgstub.pmsgheader);
    } else if (rmsgstub.isCanceled()) {

        if (rmsgstub.packet_count == 0) {
            msgswitch = PacketHeader::SwitchToOldCanceledMessageTypeE;
        } else {
            msgswitch = PacketHeader::ContinuedCanceledMessageTypeE;
        }

    } else if (rmsgstub.pmsgheader) {
        
        if(rmsgstub.serializer_ptr->empty()){
            //we've just finished serializing header
            
            _rjust_written_message_header = true;
            
            rmsgstub.serializer_ptr->push(rmsgstub.msgbundle.message_ptr, rmsgstub.msgbundle.message_type_id);
            
            rmsgstub.pmsgheader = nullptr;
            //continued message
            msgswitch = PacketHeader::PacketHeader::ContinuedMessageTypeE;        

        }else{
            
            _rcurrently_writing_message_header = true;
            
            msgswitch = PacketHeader::PacketHeader::ContinuedMessageTypeE;
        }
        
    } else if (rmsgstub.packet_count == 0) {

        //switch to old message
        msgswitch = PacketHeader::PacketHeader::SwitchToOldMessageTypeE;

    } else {

        //continued message
        msgswitch = PacketHeader::PacketHeader::ContinuedMessageTypeE;
    }
    return msgswitch;
}

//-----------------------------------------------------------------------------

void MessageWriter::forEveryMessagesNewerToOlder(VisitFunctionT const& _rvisit_fnc)
{

    { //iterate through non completed messages
        size_t msgidx = order_inner_list.frontIndex();

        while (msgidx != InvalidIndex()) {
            MessageStub& rmsgstub = message_vec[msgidx];

            if (not rmsgstub.msgbundle.message_ptr) {
                SOLID_THROW_EX("invalid message - something went wrong with the nested queue for message: ", msgidx);
                continue;
            }

            const bool message_in_write_queue = not Message::is_waiting_response(rmsgstub.msgbundle.message_flags);

            _rvisit_fnc(
                rmsgstub.msgbundle,
                rmsgstub.pool_msg_id);

            if (not rmsgstub.msgbundle.message_ptr) {

                if (message_in_write_queue) {
                    write_inner_list.erase(msgidx);
                }

                const size_t oldidx = msgidx;

                msgidx = order_inner_list.previousIndex(oldidx);

                order_inner_list.erase(oldidx);
                doUnprepareMessageStub(oldidx);

            } else {

                msgidx = order_inner_list.previousIndex(msgidx);
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
        order_inner_list.forEach(print_fnc);
        _ros << '\t';

        _ros << "WriteList: ";
        write_inner_list.forEach(print_fnc);
        _ros << '\t';

        _ros << "CacheList: ";
        cache_inner_list.forEach(print_fnc);
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
