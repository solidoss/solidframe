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
#include "solid/system/exception.hpp"

namespace solid {
namespace frame {
namespace mpipc {

//-----------------------------------------------------------------------------
MessageReader::MessageReader()
    : current_message_type_id_(InvalidIndex())
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
            SOLID_ASSERT(false);
            return;
        }
    }

    uint8_t crt_msg_type = _packet_header.type();
    bool    go_on        = false;

    //DeserializerPointerT  tmp_deserializer;

    do {

        switch (crt_msg_type) {
        case PacketHeader::MessageTypeE:
        case PacketHeader::EndMessageTypeE:
            pbufpos = _rproto.loadCrossValue(pbufpos, pbufend - pbufpos, message_idx);
            if (pbufpos and message_idx < _rconfig.max_message_count_multiplex) {
                vdbgx(Debug::mpipc, (crt_msg_type == PacketHeader::MessageTypeE ? "MessageType " : "EndMessageType ") << message_idx);
                if (message_idx >= message_vec_.size()) {
                    message_vec_.resize(message_idx + 1);
                }
                const bool is_end_of_message = (crt_msg_type == PacketHeader::EndMessageTypeE);

                pbufpos = doConsumeMessage(pbufpos, pbufend, message_idx, is_end_of_message, _receiver, _rproto, _rctx, _rerror);
            } else {
                _rerror = error_reader_protocol;
                SOLID_ASSERT(false);
                return;
            }
            break;
        case PacketHeader::CancelMessageTypeE:
            pbufpos = _rproto.loadCrossValue(pbufpos, pbufend - pbufpos, message_idx);
            vdbgx(Debug::mpipc, "CancelMessageType " << message_idx);
            if (pbufpos and message_idx < message_vec_.size()) {
                SOLID_ASSERT(message_vec_[message_idx].state_ != MessageStub::StateE::NotStarted);
                message_vec_[message_idx].clear();
            } else {
                _rerror = error_reader_protocol;
                SOLID_ASSERT(false);
            }
            break;
        case PacketHeader::KeepAliveTypeE:
            vdbgx(Debug::mpipc, "KeepAliveTypeE");
            _receiver.receiveKeepAlive();
            break;
        case PacketHeader::UpdateTypeE:
            vdbgx(Debug::mpipc, "UpdateTypeE");
            break;
        case PacketHeader::CancelRequestTypeE: {
            vdbgx(Debug::mpipc, "CancelRequestTypeE");
            RequestId requid;
            pbufpos = _rproto.loadCrossValue(pbufpos, pbufend - pbufpos, requid.index);
            if (pbufpos and (pbufpos = _rproto.loadCrossValue(pbufpos, pbufend - pbufpos, requid.unique))) {
                _receiver.receiveCancelRequest(requid);
            } else {
                _rerror = error_reader_protocol;
                SOLID_ASSERT(false);
            }
        } break;
        case PacketHeader::AckdCountTypeE:
            vdbgx(Debug::mpipc, "AckdCountTypeE");
            if (pbufpos < pbufend) {
                uint8_t count = 0;
                pbufpos       = _rproto.loadValue(pbufpos, count);
                vdbgx(Debug::mpipc, "AckdCountTypeE " << (int)count);
                _receiver.receiveAckCount(count);
            } else {
                _rerror = error_reader_protocol;
                SOLID_ASSERT(false);
            }
            break;
        default:
            _rerror = error_reader_invalid_message_switch;
            return;
        }

        SOLID_ASSERT(!_rerror);
        if (pbufpos < pbufend) {
            crt_msg_type = 0;
            pbufpos      = _rproto.loadValue(pbufpos, crt_msg_type);
            go_on        = true;
        } else {
            go_on = false;
        }
    } while (go_on and not _rerror);
}

const char* MessageReader::doConsumeMessage(
    const char*        _pbufpos,
    const char* const  _pbufend,
    const uint32_t     _msgidx,
    const bool         _is_end_of_message,
    Receiver&          _receiver,
    Protocol const&    _rproto,
    ConnectionContext& _rctx,
    ErrorConditionT&   _rerror)
{
    MessageStub& rmsgstub     = message_vec_[_msgidx];
    uint16_t     message_size = 0;

    switch (rmsgstub.state_) {
    case MessageStub::StateE::NotStarted:
        vdbgx(Debug::mpipc, "NotStarted msgidx = " << _msgidx);
        rmsgstub.deserializer_ptr_ = _rproto.createDeserializer();
        _rproto.reset(*rmsgstub.deserializer_ptr_);
        rmsgstub.deserializer_ptr_->push(rmsgstub.message_header_);
        rmsgstub.state_ = MessageStub::StateE::ReadHead;
    case MessageStub::StateE::ReadHead:
        vdbgx(Debug::mpipc, "ReadHead " << _msgidx);
        if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
            _pbufpos = _rproto.loadValue(_pbufpos, message_size);
            vdbgx(Debug::mpipc, "msgidx = " << _msgidx << " message_size = " << message_size);
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
                            rmsgstub.deserializer_ptr_->push(rmsgstub.message_ptr_);
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
        vdbgx(Debug::mpipc, "ReadBody " << _msgidx);
        ++rmsgstub.packet_count_;
        if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
            _pbufpos = _rproto.loadValue(_pbufpos, message_size);
            vdbgx(Debug::mpipc, "msgidx = " << _msgidx << " message_size = " << message_size);
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
        _rerror = error_reader_protocol;
        SOLID_ASSERT(false);
        _pbufpos = _pbufend;
        rmsgstub.clear();
        break;
    case MessageStub::StateE::RelayBody:
        SOLID_CHECK(false, "Not implemented yet. Relay to: " << rmsgstub.message_header_.url_);
        break;
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
