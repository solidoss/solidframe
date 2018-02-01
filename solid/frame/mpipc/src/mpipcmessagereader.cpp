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
size_t MessageReader::read(
    const char*      _pbuf,
    size_t           _bufsz,
    Receiver&        _receiver,
    ErrorConditionT& _rerror)
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
            const char* tmpbufpos = packet_header.load(pbufpos, _receiver.protocol());
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
    const char*         _pbuf,
    PacketHeader const& _packet_header,
    Receiver&           _receiver,
    ErrorConditionT&    _rerror)
{
    const char* pbufpos = _pbuf;
    const char* pbufend = _pbuf + _packet_header.size();
    char        tmpbuf[Protocol::MaxPacketDataSize]; //decompress = TODO: try not to use so much stack
    uint32_t    message_idx;

    if (_packet_header.isCompressed()) {
        size_t uncompressed_size = _receiver.configuration().decompress_fnc(tmpbuf, pbufpos, pbufend - pbufpos, _rerror);

        if (!_rerror) {
            pbufpos = tmpbuf;
            pbufend = tmpbuf + uncompressed_size;
        } else {
            SOLID_ASSERT(false);
            return;
        }
    }

    if (_packet_header.flags() & static_cast<uint8_t>(PacketHeader::FlagE::AckRequest)) {
        ++_receiver.request_buffer_ack_count_;
    }

    if (_packet_header.isTypeKeepAlive()) {
        vdbgx(Debug::mpipc, "KeepAliveTypeE");
        _receiver.receiveKeepAlive();
        return;
    }

    //DeserializerPointerT  tmp_deserializer;

    while (pbufpos < pbufend and not _rerror) {
        uint8_t cmd = 0;
        pbufpos     = _receiver.protocol().loadValue(pbufpos, cmd);

        switch (static_cast<PacketHeader::CommandE>(cmd)) {
        case PacketHeader::CommandE::NewMessage:
        case PacketHeader::CommandE::FullMessage:
        case PacketHeader::CommandE::Message:
        case PacketHeader::CommandE::EndMessage:
            pbufpos = _receiver.protocol().loadCrossValue(pbufpos, pbufend - pbufpos, message_idx);
            if (pbufpos and message_idx < _receiver.configuration().max_message_count_multiplex) {
                vdbgx(Debug::mpipc, "messagetype = " << (int)cmd << " msgidx = " << message_idx);
                if (message_idx >= message_vec_.size()) {
                    message_vec_.resize(message_idx + 1);
                }

                pbufpos = doConsumeMessage(pbufpos, pbufend, message_idx, cmd, _receiver, _rerror);
            } else {
                _rerror = error_reader_protocol;
                SOLID_ASSERT(false);
                return;
            }
            break;
        case PacketHeader::CommandE::CancelMessage:
            pbufpos = _receiver.protocol().loadCrossValue(pbufpos, pbufend - pbufpos, message_idx);
            edbgx(Debug::mpipc, "CancelMessage " << message_idx);
            if (pbufpos and message_idx < message_vec_.size()) {
                MessageStub& rmsgstub = message_vec_[message_idx];
                SOLID_ASSERT(rmsgstub.state_ != MessageStub::StateE::NotStarted);
                if (rmsgstub.state_ == MessageStub::StateE::RelayBody) {
                    _receiver.cancelRelayed(rmsgstub.relay_id);
                }
                rmsgstub.clear();
            } else {
                _rerror = error_reader_protocol;
                SOLID_ASSERT(false);
            }
            break;
        case PacketHeader::CommandE::Update:
            vdbgx(Debug::mpipc, "Update");
            break;
        case PacketHeader::CommandE::CancelRequest: {
            RequestId requid;
            pbufpos = _receiver.protocol().loadCrossValue(pbufpos, pbufend - pbufpos, requid.index);
            if (pbufpos and (pbufpos = _receiver.protocol().loadCrossValue(pbufpos, pbufend - pbufpos, requid.unique))) {
                vdbgx(Debug::mpipc, "CancelRequest: " << requid);
                _receiver.receiveCancelRequest(requid);
            } else {
                vdbgx(Debug::mpipc, "CancelRequest - error parsing requestid");
                _rerror = error_reader_protocol;
                SOLID_ASSERT(false);
            }
        } break;
        case PacketHeader::CommandE::AckdCount:
            vdbgx(Debug::mpipc, "AckdCount");
            if (pbufpos < pbufend) {
                uint8_t count = 0;
                pbufpos       = _receiver.protocol().loadValue(pbufpos, count);
                vdbgx(Debug::mpipc, "AckdCount " << (int)count);
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

    } //while
}
//-----------------------------------------------------------------------------
const char* MessageReader::doConsumeMessage(
    const char*       _pbufpos,
    const char* const _pbufend,
    const uint32_t    _msgidx,
    const uint8_t     _cmd,
    Receiver&         _receiver,
    ErrorConditionT&  _rerror)
{
    MessageStub& rmsgstub     = message_vec_[_msgidx];
    uint16_t     message_size = 0;

    if (_cmd & static_cast<uint8_t>(PacketHeader::CommandE::NewMessage)) {
        rmsgstub.clear();
    }

    switch (rmsgstub.state_) {
    case MessageStub::StateE::NotStarted:
        vdbgx(Debug::mpipc, "NotStarted msgidx = " << _msgidx);
        rmsgstub.deserializer_ptr_ = createDeserializer(_receiver);
        rmsgstub.deserializer_ptr_->resetLimits();
        rmsgstub.deserializer_ptr_->push(rmsgstub.message_header_);
        rmsgstub.state_ = MessageStub::StateE::ReadHead;
    case MessageStub::StateE::ReadHead:
        vdbgx(Debug::mpipc, "ReadHead " << _msgidx);
        if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
            _pbufpos = _receiver.protocol().loadValue(_pbufpos, message_size);
            vdbgx(Debug::mpipc, "msgidx = " << _msgidx << " message_size = " << message_size);
            if (message_size <= static_cast<size_t>(_pbufend - _pbufpos)) {
                _receiver.context().pmessage_header = &rmsgstub.message_header_;

                const int rv = rmsgstub.deserializer_ptr_->run(_receiver.context(), _pbufpos, message_size);
                _pbufpos += message_size;

                if (rv >= 0) {
                    if (rv <= static_cast<int>(message_size)) {

                        if (rmsgstub.deserializer_ptr_->empty()) {
                            vdbgx(Debug::mpipc, "message_size = " << message_size << " recipient_req_id = " << rmsgstub.message_header_.recipient_request_id_ << " sender_req_id = " << rmsgstub.message_header_.sender_request_id_ << " url = " << rmsgstub.message_header_.url_);

                            const ResponseStateE rsp_state = rmsgstub.message_header_.recipient_request_id_.isValid() ? _receiver.checkResponseState(rmsgstub.message_header_, rmsgstub.relay_id) : ResponseStateE::None;

                            if (rsp_state == ResponseStateE::Cancel) {
                                idbgx(Debug::mpipc, "Canceled response");
                                rmsgstub.state_ = MessageStub::StateE::IgnoreBody;
                                cache(rmsgstub.deserializer_ptr_);
                                rmsgstub.deserializer_ptr_.reset();
                            } else if (rsp_state == ResponseStateE::RelayedWait) {
                                idbgx(Debug::mpipc, "Relayed response");
                                rmsgstub.state_ = MessageStub::StateE::RelayResponse;
                                cache(rmsgstub.deserializer_ptr_);
                            } else if (_receiver.isRelayDisabled() or rmsgstub.message_header_.url_.empty()) {
                                idbgx(Debug::mpipc, "Read Body");
                                rmsgstub.state_ = MessageStub::StateE::ReadBody;
                                rmsgstub.deserializer_ptr_->clear();
                                rmsgstub.deserializer_ptr_->push(rmsgstub.message_ptr_);
                            } else {
                                idbgx(Debug::mpipc, "Relay message");
                                rmsgstub.state_ = MessageStub::StateE::RelayStart;
                                cache(rmsgstub.deserializer_ptr_);
                            }
                        }
                        break;
                    }
                } else {
                    _rerror  = rmsgstub.deserializer_ptr_->error();
                    _pbufpos = _pbufend;
                    cache(rmsgstub.deserializer_ptr_);
                    rmsgstub.clear();
                    SOLID_ASSERT(false);
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
        if (
            (rmsgstub.packet_count_ & 15) != 0 or _receiver.checkResponseState(rmsgstub.message_header_, rmsgstub.relay_id) != ResponseStateE::Cancel) {
            ++rmsgstub.packet_count_;
            if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
                _pbufpos = _receiver.protocol().loadValue(_pbufpos, message_size);
                vdbgx(Debug::mpipc, "msgidx = " << _msgidx << " message_size = " << message_size);

                if (message_size <= static_cast<size_t>(_pbufend - _pbufpos)) {

                    _receiver.context().pmessage_header = &rmsgstub.message_header_;

                    const int rv = rmsgstub.deserializer_ptr_->run(_receiver.context(), _pbufpos, message_size);
                    _pbufpos += message_size;

                    if (rv >= 0) {
                        if (rv <= static_cast<int>(message_size)) {

                            if (_cmd & static_cast<uint8_t>(PacketHeader::CommandE::EndMessageFlag)) {
                                if (rmsgstub.deserializer_ptr_->empty()) {
                                    //done parsing the message body
                                    MessagePointerT msgptr{std::move(rmsgstub.message_ptr_)};
                                    cache(rmsgstub.deserializer_ptr_);
                                    rmsgstub.clear();
                                    const size_t message_type_id = msgptr.get() ? _receiver.protocol().typeIndex(msgptr.get()) : InvalidIndex();
                                    _receiver.receiveMessage(msgptr, message_type_id);
                                    break;
                                }
                                //fail: protocol error
                                SOLID_ASSERT(false);
                            } else if (not rmsgstub.deserializer_ptr_->empty()) {
                                break;
                            } else {
                                SOLID_ASSERT(false);
                            }
                        } else {
                            SOLID_ASSERT(false);
                        }
                    } else {
                        edbgx(Debug::mpipc, "msgidx = " << _msgidx << " message_size = " << message_size);
                        _rerror = rmsgstub.deserializer_ptr_->error();
                        cache(rmsgstub.deserializer_ptr_);
                        _pbufpos = _pbufend;
                        rmsgstub.clear();
                        SOLID_ASSERT(false);
                        break;
                    }
                } else {
                    SOLID_ASSERT(false);
                }
            } else {
                SOLID_ASSERT(false);
            }

            //protocol error
            _rerror  = error_reader_protocol;
            _pbufpos = _pbufend;
            rmsgstub.clear();
            break;
        } else {
            rmsgstub.state_ = MessageStub::StateE::IgnoreBody;
            rmsgstub.deserializer_ptr_->clear();
            rmsgstub.deserializer_ptr_.reset();
        }
    case MessageStub::StateE::IgnoreBody:
        vdbgx(Debug::mpipc, "IgnoreBody " << _msgidx);
        ++rmsgstub.packet_count_;
        if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
            _pbufpos = _receiver.protocol().loadValue(_pbufpos, message_size);
            vdbgx(Debug::mpipc, "msgidx = " << _msgidx << " message_size = " << message_size);
            if (message_size <= static_cast<size_t>(_pbufend - _pbufpos)) {

                _pbufpos += message_size;

                if (_cmd & static_cast<uint8_t>(PacketHeader::CommandE::EndMessageFlag)) {
                    cache(rmsgstub.deserializer_ptr_);
                    rmsgstub.clear();
                }
                break;
            } else {
                SOLID_ASSERT(false);
            }
        } else {
            SOLID_ASSERT(false);
        }

        //protocol error
        _rerror  = error_reader_protocol;
        _pbufpos = _pbufend;
        rmsgstub.clear();
        break;
    case MessageStub::StateE::RelayStart:
        if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
            _pbufpos = _receiver.protocol().loadValue(_pbufpos, message_size);

            const bool is_message_end = (_cmd & static_cast<uint8_t>(PacketHeader::CommandE::EndMessageFlag)) != 0;
            vdbgx(Debug::mpipc, "msgidx = " << _msgidx << " message_size = " << message_size << " is_message_end = " << is_message_end);
            //TODO:
            if (_receiver.receiveRelayStart(rmsgstub.message_header_, _pbufpos, message_size, rmsgstub.relay_id, is_message_end, _rerror)) {
                rmsgstub.state_ = MessageStub::StateE::RelayBody;
            } else {
                rmsgstub.state_ = MessageStub::StateE::RelayFail;
                _receiver.pushCancelRequest(rmsgstub.message_header_.sender_request_id_);
            }
            _pbufpos += message_size;

            if (_rerror) {
                _pbufpos = _pbufend;
            }
            break;
        }
        //protocol error
        _rerror = error_reader_protocol;
        SOLID_ASSERT(false);
        _pbufpos = _pbufend;
        rmsgstub.clear();
        break;
    case MessageStub::StateE::RelayBody:
        if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
            _pbufpos = _receiver.protocol().loadValue(_pbufpos, message_size);

            vdbgx(Debug::mpipc, "msgidx = " << _msgidx << " message_size = " << message_size);
            const bool is_message_end = (_cmd & static_cast<uint8_t>(PacketHeader::CommandE::EndMessageFlag)) != 0;

            if (_receiver.receiveRelayBody(_pbufpos, message_size, rmsgstub.relay_id, is_message_end, _rerror)) {
            } else {
                rmsgstub.state_ = MessageStub::StateE::RelayFail;
                _receiver.pushCancelRequest(rmsgstub.message_header_.sender_request_id_);
            }
            _pbufpos += message_size;

            if (_rerror) {
                _pbufpos = _pbufend;
            }
            break;
        }
        //protocol error
        _rerror = error_reader_protocol;
        SOLID_ASSERT(false);
        _pbufpos = _pbufend;
        rmsgstub.clear();
        break;
    case MessageStub::StateE::RelayFail:
        if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
            _pbufpos = _receiver.protocol().loadValue(_pbufpos, message_size);
            vdbgx(Debug::mpipc, "msgidx = " << _msgidx << " message_size = " << message_size);
            _pbufpos += message_size;
            const bool is_message_end = (_cmd & static_cast<uint8_t>(PacketHeader::CommandE::EndMessageFlag)) != 0;
            if (is_message_end) {
                rmsgstub.clear();
            }
            break;
        }
        //protocol error
        _rerror = error_reader_protocol;
        SOLID_ASSERT(false);
        _pbufpos = _pbufend;
        rmsgstub.clear();
        break;
    case MessageStub::StateE::RelayResponse:
        if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
            _pbufpos = _receiver.protocol().loadValue(_pbufpos, message_size);

            vdbgx(Debug::mpipc, "msgidx = " << _msgidx << " message_size = " << message_size);
            const bool is_message_end = (_cmd & static_cast<uint8_t>(PacketHeader::CommandE::EndMessageFlag)) != 0;

            //TODO:
            if (_receiver.receiveRelayResponse(rmsgstub.message_header_, _pbufpos, message_size, rmsgstub.relay_id, is_message_end, _rerror)) {
                rmsgstub.state_ = MessageStub::StateE::RelayBody;
            } else {
                rmsgstub.state_ = MessageStub::StateE::RelayFail;
                _receiver.pushCancelRequest(rmsgstub.message_header_.sender_request_id_);
            }
            _pbufpos += message_size;

            if (_rerror) {
                _pbufpos = _pbufend;
            }
            break;
        }
        //protocol error
        _rerror = error_reader_protocol;
        SOLID_ASSERT(false);
        _pbufpos = _pbufend;
        rmsgstub.clear();
        break;
    default:
        SOLID_CHECK(false, "Invalid message state: " << (int)rmsgstub.state_);
        break;
    }
    SOLID_ASSERT(!_rerror || (_rerror && _pbufpos == _pbufend));
    return _pbufpos;
}
//-----------------------------------------------------------------------------
void MessageReader::cache(Deserializer::PointerT& _des)
{
    if (_des) {
        _des->clear();
        _des->link(des_top_);
        des_top_ = std::move(_des);
    }
}
//-----------------------------------------------------------------------------
Deserializer::PointerT MessageReader::createDeserializer(Receiver& _receiver)
{
    if (des_top_) {
        Deserializer::PointerT des{std::move(des_top_)};
        des_top_ = std::move(des->link());
        return des;
    } else {
        return _receiver.protocol().createDeserializer();
    }
}
//-----------------------------------------------------------------------------

/*virtual*/ MessageReader::Receiver::~Receiver() {}

/*virtual*/ bool MessageReader::Receiver::receiveRelayStart(MessageHeader& _rmsghdr, const char* _pbeg, size_t _sz, MessageId& _rrelay_id, const bool _is_last, ErrorConditionT& _rerror)
{
    return false;
}
/*virtual*/ bool MessageReader::Receiver::receiveRelayBody(const char* _pbeg, size_t _sz, const MessageId& _rrelay_id, const bool _is_last, ErrorConditionT& _rerror)
{
    return false;
}
/*virtual*/ bool MessageReader::Receiver::receiveRelayResponse(MessageHeader& _rmsghdr, const char* _pbeg, size_t _sz, const MessageId& _rrelay_id, const bool _is_last, ErrorConditionT& _rerror)
{
    return false;
}
/*virtual*/ ResponseStateE MessageReader::Receiver::checkResponseState(const MessageHeader& _rmsghdr, MessageId& _rrelay_id) const
{
    return ResponseStateE::None;
}
/*virtual*/ bool MessageReader::Receiver::isRelayDisabled() const
{
    return true;
}
/*virtual*/ void MessageReader::Receiver::pushCancelRequest(const RequestId&) {}
/*virtual*/ void MessageReader::Receiver::cancelRelayed(const MessageId&) {}
//-----------------------------------------------------------------------------

} //namespace mpipc
} //namespace frame
} //namespace solid
