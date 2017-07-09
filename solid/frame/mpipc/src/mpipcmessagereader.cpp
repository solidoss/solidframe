// solid/frame/ipc/src/ipcmessagereader.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "mpipcmessagereader.hpp"
#include "mpipcutility.hpp"
#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcerror.hpp"
#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/system/debug.hpp"

namespace solid {
namespace frame {
namespace mpipc {

//-----------------------------------------------------------------------------
MessageReader::MessageReader()
    : current_message_idx_(0)
    , current_message_type_id_(InvalidIndex())
    , state_(StateE::ReadPacketHead)
{
}
//-----------------------------------------------------------------------------
MessageReader::~MessageReader()
{
}
//-----------------------------------------------------------------------------
void MessageReader::prepare(ReaderConfiguration const& _rconfig)
{
    //message_vec_.push(MessageStub()); //start with an empty message
}
//-----------------------------------------------------------------------------
void MessageReader::unprepare()
{
}
//-----------------------------------------------------------------------------
uint32_t MessageReader::read(
    const char*                _pbuf,
    uint32_t                   _bufsz,
    Receiver&                  _receiver,
    ReaderConfiguration const& _rconfig,
    Protocol const&            _rproto,
    ConnectionContext&         _rctx,
    ErrorConditionT&           _rerror)
{
    const char*  pbufpos = _pbuf;
    const char*  pbufend = _pbuf + _bufsz;
    PacketHeader packet_header;

    while (pbufpos != pbufend) {
        if (state_ == StateE::ReadPacketHead) {
            //try read the header
            if ((pbufend - pbufpos) >= PacketHeader::SizeOfE) {
                state_ = StateE::ReadPacketBody;
            } else {
                break;
            }
        }

        if (state_ == StateE::ReadPacketBody) {
            //try read the data
            const char* tmpbufpos = packet_header.load(pbufpos, _rproto);
            if (static_cast<size_t>(pbufend - tmpbufpos) >= packet_header.size()) {
                pbufpos = tmpbufpos;
            } else {
                break;
            }
        }

        if (not packet_header.isOk()) {
            _rerror = error_reader_invalid_packet_header;
            SOLID_ASSERT(false);
            break;
        }

        //parse the data
        doConsumePacket(
            pbufpos,
            packet_header,
            _receiver,
            _rconfig,
            _rproto,
            _rctx,
            _rerror);

        if (!_rerror) {
            pbufpos += packet_header.size();
        } else {
            SOLID_ASSERT(false);
            break;
        }
        state_ = StateE::ReadPacketHead;
    }

    return pbufpos - _pbuf;
}
//-----------------------------------------------------------------------------
//TODO: change the CHECKs below to propper protocol errors.
void MessageReader::doConsumePacket(
    const char*                _pbuf,
    PacketHeader const&        _packet_header,
    Receiver&                  _receiver,
    ReaderConfiguration const& _rconfig,
    Protocol const&            _rproto,
    ConnectionContext&         _rctx,
    ErrorConditionT&           _rerror)
{
    const char* pbufpos = _pbuf;
    const char* pbufend = _pbuf + _packet_header.size();
    char        tmpbuf[Protocol::MaxPacketDataSize]; //decompress = TODO: try not to use so much stack
    uint32_t    message_idx;

    if (_packet_header.isCompressed()) {
        size_t uncompressed_size = _rconfig.decompress_fnc(tmpbuf, pbufpos, pbufend - pbufpos, _rerror);

        if (!_rerror) {
            pbufpos = tmpbuf;
            pbufend = tmpbuf + uncompressed_size;
        } else {
            return;
        }
    }

    uint8_t crt_msg_type = _packet_header.type();

    if (_packet_header.flags() & PacketHeader::AckCountFlagE and pbufpos < pbufend) {
        uint8_t count = 0;
        pbufpos       = _rproto.loadValue(pbufpos, count);
        _receiver.receiveAckCount(count);
    }

    //DeserializerPointerT  tmp_deserializer;

    while (pbufpos < pbufend and not _rerror) {

        switch (crt_msg_type) {
        case PacketHeader::MessageTypeE:
        case PacketHeader::EndMessageTypeE:
            pbufpos = _rproto.loadCrossValue(pbufpos, pbufend - pbufpos, message_idx);
            if (pbufpos and message_idx < _rconfig.max_message_count_multiplex) {
                if (message_idx >= message_vec_.size()) {
                    message_vec_.resize(message_idx + 1);
                }

            } else {
                _rerror = error_reader_protocol;
                return;
            }
        case PacketHeader::CancelMessageTypeE:
            pbufpos = _rproto.loadCrossValue(pbufpos, pbufend - pbufpos, message_idx);
            if (pbufpos and message_idx < message_vec_.size()) {
                message_vec_[message_idx].clear();
            } else {
                _rerror = error_reader_protocol;
            }
            return;
        case PacketHeader::KeepAliveTypeE: {
            _receiver.receiveKeepAlive();
        }
            return;
        case PacketHeader::UpdateTypeE: {
        }
            return;
        default:
            _rerror = error_reader_invalid_message_switch;
            return;
        }
#if 0       
        switch (crt_msg_type) {
        case PacketHeader::SwitchToNewMessageTypeE:
        case PacketHeader::SwitchToNewEndMessageTypeE:

            vdbgx(Debug::mpipc, "SwitchToNewMessageTypeE " << message_q_.size());

            SOLID_CHECK(message_q_.size(), "message_q should not be empty");

            if (message_q_.front().state_ != MessageStub::StateE::NotStarted) {
                if (message_q_.size() == _rconfig.max_message_count_multiplex) {
                    _rerror = error_reader_too_many_multiplex;
                    return;
                }
                message_q_.front().packet_count_ = 0;
                //reschedule the current message for later
                message_q_.push(std::move(message_q_.front()));
            }

            if (not message_q_.front().deserializer_ptr_) {
                message_q_.front().deserializer_ptr_ = _rproto.createDeserializer();
            }

            _rproto.reset(*message_q_.front().deserializer_ptr_);

            message_q_.front().deserializer_ptr_->push(message_q_.front().message_header_);
            message_q_.front().state_ = MessageStub::StateE::ReadHead;
            break;

        case PacketHeader::SwitchToOldMessageTypeE:
        case PacketHeader::SwitchToOldEndMessageTypeE:

            vdbgx(Debug::mpipc, "SwitchToOldMessageTypeE " << message_q_.size());

            SOLID_CHECK(message_q_.size(), "message_q should not be empty");

            if (message_q_.front().state_ != MessageStub::StateE::NotStarted) {
                message_q_.push(std::move(message_q_.front()));
                message_q_.front().packet_count_ = 0;
            }

            message_q_.pop();

            SOLID_CHECK(message_q_.size(), "message_q should not be empty");
            break;

        case PacketHeader::ContinuedMessageTypeE:
        case PacketHeader::ContinuedEndMessageTypeE:

            vdbgx(Debug::mpipc, "ContinuedMessageTypeE " << message_q_.size());

            SOLID_CHECK(message_q_.size() and message_q_.front().state_ != MessageStub::StateE::NotStarted, "No message to continue");

            if (message_q_.front().state_ != MessageStub::StateE::ReadHead) {
                ++message_q_.front().packet_count_;
            }
            break;

        case PacketHeader::SwitchToOldCanceledMessageTypeE:

            vdbgx(Debug::mpipc, "SwitchToOldCanceledMessageTypeE " << message_q_.size());

            SOLID_CHECK(message_q_.size(), "message_q should not be empty");

            if (message_q_.front().state_ != MessageStub::StateE::NotStarted) {
                message_q_.push(std::move(message_q_.front()));
                message_q_.front().packet_count_ = 0;
            }
            message_q_.pop();

            SOLID_CHECK(message_q_.size(), "message_q should not be empty");

            message_q_.front().state_ = MessageStub::StateE::Canceled;
            break;

        case PacketHeader::ContinuedCanceledMessageTypeE:

            vdbgx(Debug::mpipc, "ContinuedCanceledMessageTypeE " << message_q_.size());

            SOLID_CHECK(message_q_.size() and message_q_.front().state_ != MessageStub::StateE::NotStarted, "No message to cancel");

            message_q_.front().state_ = MessageStub::StateE::Canceled;
            break;
        case PacketHeader::KeepAliveTypeE:{
            _receiver.receiveKeepAlive();
        }   return;
        case PacketHeader::UpdateTypeE:{
        }   return;
        default:
            _rerror = error_reader_invalid_message_switch;
            return;
        }
#endif
        SOLID_CHECK(message_vec_.size(), "message_q should not be empty");

        const bool is_end_of_message = (crt_msg_type == PacketHeader::EndMessageTypeE);

        pbufpos = doConsumeMessage(pbufpos, pbufend, is_end_of_message, _receiver, _rproto, _rctx, _rerror);

        if (pbufpos < pbufend) {
            crt_msg_type = 0;
            pbufpos      = _rproto.loadValue(pbufpos, crt_msg_type);
        }
    } //while
}

const char* MessageReader::doConsumeMessage(
    const char*        _pbufpos,
    const char* const  _pbufend,
    const bool         _is_end_of_message,
    Receiver&          _receiver,
    Protocol const&    _rproto,
    ConnectionContext& _rctx,
    ErrorConditionT&   _rerror)
{
    MessageStub& rmsgstub     = message_q_.front();
    uint16_t     message_size = 0;

    switch (rmsgstub.state_) {
    case MessageStub::StateE::ReadHead:
        if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
            _pbufpos = _rproto.loadValue(_pbufpos, message_size);

            if (message_size <= static_cast<size_t>(_pbufend - _pbufpos)) {
                _rctx.pmessage_header = &rmsgstub.message_header_;

                const int rv = rmsgstub.deserializer_ptr_->run(_rctx, _pbufpos, message_size);
                _pbufpos += message_size;

                if (rv == static_cast<int>(message_size)) {

                    if (rmsgstub.deserializer_ptr_->empty()) {
                        //done reading message header
                        if (rmsgstub.message_header_.url_.empty()) {
                            rmsgstub.state_ = MessageStub::StateE::ReadBody;
                            rmsgstub.deserializer_ptr_->clear();
                            message_q_.front().deserializer_ptr_->push(rmsgstub.message_ptr_);
                        } else {
                            vdbgx(Debug::mpipc, "Relay message: " << rmsgstub.message_header_.url_);
                            rmsgstub.state_ = MessageStub::StateE::RelayBody;
                            rmsgstub.deserializer_ptr_->clear();
                            rmsgstub.deserializer_ptr_.reset();
                        }
                    }

                    break;
                } else if (rv < 0) {
                    _rerror  = rmsgstub.deserializer_ptr_->error();
                    _pbufpos = _pbufend;
                    rmsgstub.clear();
                    break;
                }
            }
        }

        //protocol error
        _rerror  = error_reader_protocol;
        _pbufpos = _pbufend;
        rmsgstub.clear();
        break;
    case MessageStub::StateE::ReadBody:
        if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
            _pbufpos = _rproto.loadValue(_pbufpos, message_size);

            if (message_size <= static_cast<size_t>(_pbufend - _pbufpos)) {
                _rctx.pmessage_header = &rmsgstub.message_header_;

                const int rv = rmsgstub.deserializer_ptr_->run(_rctx, _pbufpos, message_size);
                _pbufpos += message_size;

                if (rv == static_cast<int>(message_size)) {

                    if (_is_end_of_message) {
                        if (rmsgstub.deserializer_ptr_->empty()) {
                            //done parsing the message body
                            MessagePointerT msgptr{std::move(rmsgstub.message_ptr_)};
                            rmsgstub.clear();
                            const size_t message_type_id = msgptr.get() ? _rproto.typeIndex(msgptr.get()) : InvalidIndex();
                            _receiver.receiveMessage(msgptr, message_type_id);
                            break;
                        }
                        //fail: protocol error
                    } else if (not rmsgstub.deserializer_ptr_->empty()) {
                        break;
                    }
                } else if (rv < 0) {
                    _rerror  = rmsgstub.deserializer_ptr_->error();
                    _pbufpos = _pbufend;
                    rmsgstub.clear();
                    break;
                }
            }
        }

        //protocol error
        _rerror  = error_reader_protocol;
        _pbufpos = _pbufend;
        rmsgstub.clear();
        break;
    case MessageStub::StateE::RelayBody:
        SOLID_CHECK(false, "Not implemented yet. Relay to: " << rmsgstub.message_header_.url_);
        break;
    case MessageStub::StateE::Canceled:
        rmsgstub.clear();
        break;
    case MessageStub::StateE::NotStarted:
    default:
        SOLID_CHECK(false, "Invalid message state: " << (int)rmsgstub.state_);
        break;
    }
    SOLID_ASSERT(!_rerror || (_rerror && _pbufpos == _pbufend));
    return _pbufpos;
}

} //namespace mpipc
} //namespace frame
} //namespace solid
