// solid/frame/ipc/src/ipcmessagereader.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "mprpcmessagereader.hpp"
#include "mprpcutility.hpp"
#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcerror.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"

namespace solid {
namespace frame {
namespace mprpc {
namespace {
const LoggerT logger("solid::frame::mprpc::reader");
}
//-----------------------------------------------------------------------------
MessageReader::~MessageReader()
{
}
//-----------------------------------------------------------------------------
void MessageReader::prepare(ReaderConfiguration const& /*_rconfig*/)
{
    // message_vec_.push(MessageStub()); //start with an empty message
}
//-----------------------------------------------------------------------------
void MessageReader::unprepare()
{
}
//-----------------------------------------------------------------------------
size_t MessageReader::read(
    const char*            _pbuf,
    size_t                 _bufsz,
    MessageReaderReceiver& _receiver,
    ErrorConditionT&       _rerror)
{
    const char*  pbufpos = _pbuf;
    const char*  pbufend = _pbuf + _bufsz;
    PacketHeader packet_header;

    while (pbufpos != pbufend) {
        if (state_ == StateE::ReadPacketHead) {
            // try read the header
            if ((pbufend - pbufpos) >= PacketHeader::size_of_header) {
                state_ = StateE::ReadPacketBody;
            } else {
                break;
            }
        }

        if (state_ == StateE::ReadPacketBody) {
            // try read the data
            const char* tmpbufpos = packet_header.load(pbufpos, _receiver.protocol());
            if (!packet_header.isOk()) {
                _rerror = error_reader_invalid_packet_header;
                solid_log(logger, Error, _rerror.message());
                break;
            }
            if (static_cast<size_t>(pbufend - tmpbufpos) >= packet_header.size()) {
                pbufpos = tmpbufpos;
            } else {
                break;
            }
        }

        // parse the data
        doConsumePacket(pbufpos, packet_header, _receiver, _rerror);

        if (!_rerror) {
            pbufpos += packet_header.size();
        } else {
            solid_log(logger, Error, "relay_enabled: " << _receiver.isRelayEnabled() << " consume packet error: " << _rerror.message());
            break;
        }
        state_ = StateE::ReadPacketHead;
    }

    return pbufpos - _pbuf;
}
//-----------------------------------------------------------------------------
void MessageReader::doConsumePacketLoop(
    const char*            _pbufpos,
    const char* const      _pbufend,
    MessageReaderReceiver& _receiver,
    ErrorConditionT&       _rerror)
{
    uint32_t message_idx = InvalidIndex{};

    while (_pbufpos < _pbufend && !_rerror) {
        uint8_t cmd = 0;
        _pbufpos    = _receiver.protocol().loadValue(_pbufpos, cmd);

        switch (static_cast<PacketHeader::CommandE>(cmd)) {
        case PacketHeader::CommandE::NewMessage:
        case PacketHeader::CommandE::FullMessage:
        case PacketHeader::CommandE::Message:
        case PacketHeader::CommandE::EndMessage:
            _pbufpos = _receiver.protocol().loadCompactValue(_pbufpos, _pbufend - _pbufpos, message_idx);
            if (_pbufpos != nullptr && message_idx < _receiver.configuration().max_message_count_multiplex) {
                solid_log(logger, Verbose, "messagetype = " << (int)cmd << " msgidx = " << message_idx);
                if (message_idx >= message_vec_.size()) {
                    message_vec_.resize(message_idx + 1);
                }

                _pbufpos = doConsumeMessage(_pbufpos, _pbufend, message_idx, cmd, _receiver, _rerror);
            } else {
                _rerror = error_reader_protocol;
                return;
            }
            break;
        case PacketHeader::CommandE::CancelMessage:
            _pbufpos = _receiver.protocol().loadCompactValue(_pbufpos, _pbufend - _pbufpos, message_idx);
            solid_log(logger, Error, "CancelMessage " << message_idx);
            if (_pbufpos != nullptr && message_idx < message_vec_.size()) {
                MessageStub& rmsgstub = message_vec_[message_idx];
                solid_assert_log(rmsgstub.state_ != MessageStub::StateE::NotStarted, logger);
                if (rmsgstub.state_ == MessageStub::StateE::RelayBody) {
                    _receiver.cancelRelayed(rmsgstub.relay_id);
                }
                rmsgstub.clear();
            } else {
                _rerror = error_reader_protocol;
            }
            break;
        case PacketHeader::CommandE::Update:
            solid_log(logger, Verbose, "Update");
            break;
        case PacketHeader::CommandE::CancelRequest: {
            RequestId requid;
            _pbufpos = _receiver.protocol().loadCompactValue(_pbufpos, _pbufend - _pbufpos, requid.index);
            if (_pbufpos != nullptr && (_pbufpos = _receiver.protocol().loadCompactValue(_pbufpos, _pbufend - _pbufpos, requid.unique)) != nullptr) {
                solid_log(logger, Verbose, "CancelRequest: " << requid);
                _receiver.receiveCancelRequest(requid);
            } else {
                solid_log(logger, Error, "parsing requestid");
                _rerror = error_reader_protocol;
            }
        } break;
        case PacketHeader::CommandE::AckdCount:
            solid_log(logger, Verbose, "AckdCount");
            if (_pbufpos < _pbufend) {
                uint8_t count = 0;

                _pbufpos = _receiver.protocol().loadValue(_pbufpos, count);
                solid_log(logger, Verbose, "AckdCount " << (int)count);
                _receiver.receiveAckCount(count);
            } else {
                _rerror = error_reader_protocol;
            }
            break;
        default:
            _rerror = error_reader_invalid_message_switch;
            return;
        }

    } // while
}
//-----------------------------------------------------------------------------
// TODO: change the CHECKs below to propper protocol errors.
void MessageReader::doConsumePacket(
    const char*            _pbuf,
    PacketHeader const&    _packet_header,
    MessageReaderReceiver& _receiver,
    ErrorConditionT&       _rerror)
{
    const char* pbufpos = _pbuf;
    const char* pbufend = _pbuf + _packet_header.size();

    if ((_packet_header.flags() & static_cast<uint8_t>(PacketHeader::FlagE::AckRequest)) != 0u) {
        ++_receiver.request_buffer_ack_count_;
    }

    if (_packet_header.isTypeKeepAlive()) {
        solid_log(logger, Verbose, "KeepAliveTypeE");
        _receiver.receiveKeepAlive();
        return;
    }

    if (!_packet_header.isCompressed()) [[likely]] {
        doConsumePacketLoop(pbufpos, pbufend, _receiver, _rerror);
    } else {
        char         tmpbuf[Protocol::MaxPacketDataSize]; // decompress = TODO: try not to use so much stack
        const size_t uncompressed_size = _receiver.configuration().decompress_fnc(tmpbuf, pbufpos, pbufend - pbufpos, _rerror);

        if (!_rerror) {
            pbufpos = tmpbuf;
            pbufend = tmpbuf + uncompressed_size;
        } else {
            solid_log(logger, Error, "decompressing: " << _rerror.message());
            return;
        }
        doConsumePacketLoop(pbufpos, pbufend, _receiver, _rerror);
    }
}
//-----------------------------------------------------------------------------
bool MessageReader::doConsumeMessageHeader(
    const char*&           _pbufpos,
    const char* const      _pbufend,
    const uint32_t         _msgidx,
    uint16_t&              _message_size,
    MessageReaderReceiver& _receiver,
    ErrorConditionT&       _rerror)
{
    MessageStub& rmsgstub = message_vec_[_msgidx];

    solid_log(logger, Verbose, "ReadHead " << _msgidx);
    if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
        _pbufpos = _receiver.protocol().loadValue(_pbufpos, _message_size);
        solid_log(logger, Verbose, "msgidx = " << _msgidx << " message_size = " << _message_size);
        if (_message_size <= static_cast<size_t>(_pbufend - _pbufpos)) {
            _receiver.context().pmessage_header = &rmsgstub.message_header_;

            const ptrdiff_t rv = rmsgstub.state_ == MessageStub::StateE::ReadHeadStart ? rmsgstub.deserializer_ptr_->run(_receiver.context(), _pbufpos, _message_size, rmsgstub.message_header_) : rmsgstub.deserializer_ptr_->run(_receiver.context(), _pbufpos, _message_size);

            rmsgstub.state_ = MessageStub::StateE::ReadHeadContinue;
            _pbufpos += _message_size;

            if (rv >= 0) {
                if (rv <= static_cast<long>(_message_size)) {

                    if (rmsgstub.deserializer_ptr_->empty()) {
                        const bool           valid_recipient_request_id = rmsgstub.message_header_.recipient_request_id_.isValid();
                        const bool           erase_the_awaiting_request = !Message::is_response_part(rmsgstub.message_header_.flags_);
                        const ResponseStateE rsp_state                  = valid_recipient_request_id ? _receiver.checkResponseState(rmsgstub.message_header_, rmsgstub.relay_id, erase_the_awaiting_request) : ResponseStateE::None;

                        solid_log(logger, Verbose, "msgidx = " << _msgidx << " message_size = " << _message_size << " recipient_req_id = " << rmsgstub.message_header_.recipient_request_id_ << " sender_req_id = " << rmsgstub.message_header_.sender_request_id_ << " relay = [" << rmsgstub.message_header_.relay_ << "] rsp_state = " << static_cast<int>(rsp_state));

                        if (rsp_state == ResponseStateE::Cancel) {
                            solid_log(logger, Info, "Canceled response");
                            rmsgstub.state_ = MessageStub::StateE::IgnoreBody;
                            cache(rmsgstub.deserializer_ptr_);
                            rmsgstub.deserializer_ptr_.reset();
                        } else if (rsp_state == ResponseStateE::RelayedWait) {
                            solid_log(logger, Info, "Relayed response");
                            rmsgstub.state_ = MessageStub::StateE::RelayResponse;
                            cache(rmsgstub.deserializer_ptr_);
                        } else if (!_receiver.isRelayEnabled() || rmsgstub.message_header_.relay_.empty()) {
                            solid_log(logger, Info, "Read Body");
                            rmsgstub.state_ = MessageStub::StateE::ReadBodyStart;
                            rmsgstub.deserializer_ptr_->clear();
                            _receiver.protocol().reconfigure(*rmsgstub.deserializer_ptr_, _receiver.configuration());
                        } else {
                            solid_log(logger, Info, "Relay message");
                            rmsgstub.state_ = MessageStub::StateE::RelayStart;
                            cache(rmsgstub.deserializer_ptr_);
                        }
                    }
                    return true;
                }
            } else {
                _rerror  = rmsgstub.deserializer_ptr_->error();
                _pbufpos = _pbufend;
                cache(rmsgstub.deserializer_ptr_);
                solid_log(logger, Error, "Clear Message " << _msgidx);
                rmsgstub.clear();
                solid_log(logger, Error, "deserializing: " << _rerror.message());
                return true;
            }
        }
    }
    return false;
}
//-----------------------------------------------------------------------------
bool MessageReader::doConsumeMessageBody(
    const char*&           _pbufpos,
    const char* const      _pbufend,
    const uint32_t         _msgidx,
    const uint8_t          _cmd,
    uint16_t&              _message_size,
    MessageReaderReceiver& _receiver,
    ErrorConditionT&       _rerror)
{
    MessageStub& rmsgstub = message_vec_[_msgidx];

    solid_log(logger, Verbose, "ReadBody " << _msgidx);
    if (
        (rmsgstub.packet_count_ & 15) != 0 || _receiver.checkResponseState(rmsgstub.message_header_, rmsgstub.relay_id) != ResponseStateE::Cancel) {
        ++rmsgstub.packet_count_;
        if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
            _pbufpos = _receiver.protocol().loadValue(_pbufpos, _message_size);
            solid_log(logger, Verbose, "msgidx = " << _msgidx << " message_size = " << _message_size);

            if (_message_size <= static_cast<size_t>(_pbufend - _pbufpos)) {

                _receiver.context().pmessage_header = &rmsgstub.message_header_;

                const ptrdiff_t rv = rmsgstub.state_ == MessageStub::StateE::ReadBodyStart ? rmsgstub.deserializer_ptr_->run(_receiver.context(), _pbufpos, _message_size, rmsgstub.message_ptr_) : rmsgstub.deserializer_ptr_->run(_receiver.context(), _pbufpos, _message_size);

                rmsgstub.state_ = MessageStub::StateE::ReadBodyContinue;
                _pbufpos += _message_size;

                if (rv >= 0) {
                    if (rv <= static_cast<int>(_message_size)) {

                        if ((_cmd & static_cast<uint8_t>(PacketHeader::CommandE::EndMessageFlag)) != 0u) {
                            if (rmsgstub.deserializer_ptr_->empty() && rmsgstub.message_ptr_) {
                                // done parsing the message body
                                MessagePointerT<> msgptr{std::move(rmsgstub.message_ptr_)};
                                cache(rmsgstub.deserializer_ptr_);
                                solid_log(logger, Verbose, "Clear Message " << _msgidx);
                                rmsgstub.clear();
                                const size_t message_type_id = msgptr ? _receiver.protocol().typeIndex(msgptr.get()) : InvalidIndex();
                                _receiver.receiveMessage(msgptr, message_type_id);
                                return true;
                            }
                            // fail: protocol error
                            solid_log(logger, Error, "protocol");
                        } else if (!rmsgstub.deserializer_ptr_->empty()) {
                            return true;
                        } else {
                            solid_log(logger, Error, "protocol");
                        }
                    } else {
                        solid_log(logger, Error, "protocol");
                    }
                } else {
                    _rerror = rmsgstub.deserializer_ptr_->error();
                    solid_log(logger, Error, "msgidx = " << _msgidx << " message_size = " << _message_size << " error: " << _rerror.message());
                    cache(rmsgstub.deserializer_ptr_);
                    _pbufpos = _pbufend;
                    solid_log(logger, Error, "Clear Message " << _msgidx);
                    rmsgstub.clear();
                    return true;
                }
            } else {
                solid_log(logger, Error, "protocol");
            }
        } else {
            solid_log(logger, Error, "protocol");
        }

        // protocol error
        _rerror  = error_reader_protocol;
        _pbufpos = _pbufend;
        rmsgstub.clear();
        return true;
    }
    return false;
}
//-----------------------------------------------------------------------------
bool MessageReader::doConsumeMessageIgnoreBody(
    const char*&           _pbufpos,
    const char* const      _pbufend,
    const uint32_t         _msgidx,
    const uint8_t          _cmd,
    uint16_t&              _message_size,
    MessageReaderReceiver& _receiver)
{
    solid_log(logger, Verbose, "IgnoreBody " << _msgidx);

    MessageStub& rmsgstub = message_vec_[_msgidx];

    ++rmsgstub.packet_count_;

    if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
        _pbufpos = _receiver.protocol().loadValue(_pbufpos, _message_size);
        solid_log(logger, Verbose, "msgidx = " << _msgidx << " message_size = " << _message_size);
        if (_message_size <= static_cast<size_t>(_pbufend - _pbufpos)) {

            _pbufpos += _message_size;

            if ((_cmd & static_cast<uint8_t>(PacketHeader::CommandE::EndMessageFlag)) != 0u) {
                cache(rmsgstub.deserializer_ptr_);
                solid_log(logger, Error, "Clear Message " << _msgidx);
                rmsgstub.clear();
            }
            return true;
        }
        solid_log(logger, Error, "protocol");
    } else {
        solid_log(logger, Error, "protocol");
    }
    return false;
}
//-----------------------------------------------------------------------------
void MessageReader::doConsumeMessageRelayStart(
    const char*&           _pbufpos,
    const char* const      _pbufend,
    const uint32_t         _msgidx,
    const uint8_t          _cmd,
    uint16_t&              _message_size,
    MessageReaderReceiver& _receiver,
    ErrorConditionT&       _rerror)
{
    MessageStub& rmsgstub = message_vec_[_msgidx];

    if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
        _pbufpos = _receiver.protocol().loadValue(_pbufpos, _message_size);

        const bool is_message_end = (_cmd & static_cast<uint8_t>(PacketHeader::CommandE::EndMessageFlag)) != 0;
        solid_log(logger, Verbose, "msgidx = " << _msgidx << " message_size = " << _message_size << " is_message_end = " << is_message_end);
        // TODO:
        if (_receiver.receiveRelayStart(rmsgstub.message_header_, _pbufpos, _message_size, rmsgstub.relay_id, is_message_end, _rerror)) {
            rmsgstub.state_ = MessageStub::StateE::RelayBody;
        } else {
            rmsgstub.state_ = MessageStub::StateE::RelayFail;
            _receiver.pushCancelRequest(rmsgstub.message_header_.sender_request_id_);
        }
        _pbufpos += _message_size;

        if (_rerror) {
            _pbufpos = _pbufend;
        }
        return;
    }
    // protocol error
    _rerror = error_reader_protocol;
    solid_log(logger, Error, "protocol");
    _pbufpos = _pbufend;
    solid_log(logger, Error, "Clear Message " << _msgidx);
    rmsgstub.clear();
}
//-----------------------------------------------------------------------------
void MessageReader::doConsumeMessageRelayBody(
    const char*&           _pbufpos,
    const char* const      _pbufend,
    const uint32_t         _msgidx,
    const uint8_t          _cmd,
    uint16_t&              _message_size,
    MessageReaderReceiver& _receiver,
    ErrorConditionT&       _rerror)
{
    MessageStub& rmsgstub = message_vec_[_msgidx];

    if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
        _pbufpos = _receiver.protocol().loadValue(_pbufpos, _message_size);

        solid_log(logger, Verbose, "msgidx = " << _msgidx << " message_size = " << _message_size);
        const bool is_message_end = (_cmd & static_cast<uint8_t>(PacketHeader::CommandE::EndMessageFlag)) != 0;

        if (_receiver.receiveRelayBody(_pbufpos, _message_size, rmsgstub.relay_id, is_message_end, _rerror)) {
        } else {
            rmsgstub.state_ = MessageStub::StateE::RelayFail;
            _receiver.pushCancelRequest(rmsgstub.message_header_.sender_request_id_);
        }
        _pbufpos += _message_size;

        if (_rerror) {
            _pbufpos = _pbufend;
        }
        return;
    }
    // protocol error
    _rerror = error_reader_protocol;
    solid_log(logger, Error, "protocol");
    _pbufpos = _pbufend;
    solid_log(logger, Error, "Clear Message " << _msgidx);
    rmsgstub.clear();
}
//-----------------------------------------------------------------------------
void MessageReader::doConsumeMessageRelayFail(
    const char*&           _pbufpos,
    const char* const      _pbufend,
    const uint32_t         _msgidx,
    const uint8_t          _cmd,
    uint16_t&              _message_size,
    MessageReaderReceiver& _receiver,
    ErrorConditionT&       _rerror)
{
    MessageStub& rmsgstub = message_vec_[_msgidx];

    if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
        _pbufpos = _receiver.protocol().loadValue(_pbufpos, _message_size);
        solid_log(logger, Verbose, "msgidx = " << _msgidx << " message_size = " << _message_size);
        _pbufpos += _message_size;
        const bool is_message_end = (_cmd & static_cast<uint8_t>(PacketHeader::CommandE::EndMessageFlag)) != 0;
        if (is_message_end) {
            solid_log(logger, Error, "Clear Message " << _msgidx);
            rmsgstub.clear();
        }
        return;
    }
    // protocol error
    _rerror = error_reader_protocol;
    solid_log(logger, Error, "protocol");
    _pbufpos = _pbufend;
    solid_log(logger, Error, "Clear Message " << _msgidx);
    rmsgstub.clear();
}
//-----------------------------------------------------------------------------
void MessageReader::doConsumeMessageRelayResponse(
    const char*&           _pbufpos,
    const char* const      _pbufend,
    const uint32_t         _msgidx,
    const uint8_t          _cmd,
    uint16_t&              _message_size,
    MessageReaderReceiver& _receiver,
    ErrorConditionT&       _rerror)
{
    MessageStub& rmsgstub = message_vec_[_msgidx];
    if (static_cast<size_t>(_pbufend - _pbufpos) >= sizeof(uint16_t)) {
        _pbufpos = _receiver.protocol().loadValue(_pbufpos, _message_size);

        solid_log(logger, Verbose, "msgidx = " << _msgidx << " message_size = " << _message_size);
        const bool is_message_end = (_cmd & static_cast<uint8_t>(PacketHeader::CommandE::EndMessageFlag)) != 0;

        // TODO:
        if (_receiver.receiveRelayResponse(rmsgstub.message_header_, _pbufpos, _message_size, rmsgstub.relay_id, is_message_end, _rerror)) {
            rmsgstub.state_ = MessageStub::StateE::RelayBody;
        } else {
            rmsgstub.state_ = MessageStub::StateE::RelayFail;
            _receiver.pushCancelRequest(rmsgstub.message_header_.sender_request_id_);
        }
        _pbufpos += _message_size;

        if (_rerror) {
            _pbufpos = _pbufend;
        }
        return;
    }
    // protocol error
    _rerror = error_reader_protocol;
    solid_log(logger, Error, "protocol");
    _pbufpos = _pbufend;
    solid_log(logger, Error, "Clear Message " << _msgidx);
    rmsgstub.clear();
}
//-----------------------------------------------------------------------------
const char* MessageReader::doConsumeMessage(
    const char*            _pbufpos,
    const char* const      _pbufend,
    const uint32_t         _msgidx,
    const uint8_t          _cmd,
    MessageReaderReceiver& _receiver,
    ErrorConditionT&       _rerror)
{
    MessageStub& rmsgstub     = message_vec_[_msgidx];
    uint16_t     message_size = 0;

    if ((_cmd & static_cast<uint8_t>(PacketHeader::CommandE::NewMessage)) != 0u) {
        solid_log(logger, Verbose, "Clear Message " << _msgidx);
        rmsgstub.clear();
    }

    switch (rmsgstub.state_) {
    case MessageStub::StateE::NotStarted:
        solid_log(logger, Verbose, "NotStarted msgidx = " << _msgidx);
        rmsgstub.deserializer_ptr_ = createDeserializer(_receiver);
        rmsgstub.state_            = MessageStub::StateE::ReadHeadStart;
    case MessageStub::StateE::ReadHeadStart:
    case MessageStub::StateE::ReadHeadContinue:
        if (!doConsumeMessageHeader(_pbufpos, _pbufend, _msgidx, message_size, _receiver, _rerror)) {
            // protocol error
            _rerror  = error_reader_protocol;
            _pbufpos = _pbufend;
            solid_log(logger, Error, "Clear Message " << _msgidx);
            rmsgstub.clear();
        }
        break;
    case MessageStub::StateE::ReadBodyStart:
    case MessageStub::StateE::ReadBodyContinue:
        if (doConsumeMessageBody(_pbufpos, _pbufend, _msgidx, _cmd, message_size, _receiver, _rerror)) {
            break;
        }
        rmsgstub.state_ = MessageStub::StateE::IgnoreBody;
        rmsgstub.deserializer_ptr_->clear();
        rmsgstub.deserializer_ptr_.reset();
    case MessageStub::StateE::IgnoreBody:
        if (!doConsumeMessageIgnoreBody(_pbufpos, _pbufend, _msgidx, _cmd, message_size, _receiver)) {
            // protocol error
            _rerror  = error_reader_protocol;
            _pbufpos = _pbufend;
            rmsgstub.clear();
            solid_log(logger, Error, "Clear Message " << _msgidx);
        }
        break;
    case MessageStub::StateE::RelayStart:
        doConsumeMessageRelayStart(_pbufpos, _pbufend, _msgidx, _cmd, message_size, _receiver, _rerror);
        break;
    case MessageStub::StateE::RelayBody:
        doConsumeMessageRelayBody(_pbufpos, _pbufend, _msgidx, _cmd, message_size, _receiver, _rerror);
        break;
    case MessageStub::StateE::RelayFail:
        doConsumeMessageRelayFail(_pbufpos, _pbufend, _msgidx, _cmd, message_size, _receiver, _rerror);
        break;
    case MessageStub::StateE::RelayResponse:
        doConsumeMessageRelayResponse(_pbufpos, _pbufend, _msgidx, _cmd, message_size, _receiver, _rerror);
        break;
    default:
        solid_check_log(false, logger, "Invalid message state: " << (int)rmsgstub.state_);
        break;
    }
    solid_assert_log(!_rerror || (_rerror && _pbufpos == _pbufend), logger);
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
Deserializer::PointerT MessageReader::createDeserializer(MessageReaderReceiver& _receiver)
{
    if (des_top_) {
        Deserializer::PointerT des{std::move(des_top_)};
        des_top_ = std::move(des->link());
        _receiver.protocol().reconfigure(*des, _receiver.configuration());
        return des;
    }
    return _receiver.protocol().createDeserializer(_receiver.configuration());
}
//-----------------------------------------------------------------------------

/*virtual*/ MessageReaderReceiver::~MessageReaderReceiver() {}

/*virtual*/ bool MessageReaderReceiver::receiveRelayStart(MessageHeader& /*_rmsghdr*/, const char* /*_pbeg*/, size_t /*_sz*/, MessageId& /*_rrelay_id*/, const bool /*_is_last*/, ErrorConditionT& /*_rerror*/)
{
    solid_assert(false);
    return false;
}
/*virtual*/ bool MessageReaderReceiver::receiveRelayBody(const char* /*_pbeg*/, size_t /*_sz*/, const MessageId& /*_rrelay_id*/, const bool /*_is_last*/, ErrorConditionT& /*_rerror*/)
{
    solid_assert(false);
    return false;
}
/*virtual*/ bool MessageReaderReceiver::receiveRelayResponse(MessageHeader& /*_rmsghdr*/, const char* /*_pbeg*/, size_t /*_sz*/, const MessageId& /*_rrelay_id*/, const bool /*_is_last*/, ErrorConditionT& /*_rerror*/)
{
    solid_assert(false);
    return false;
}
/*virtual*/ ResponseStateE MessageReaderReceiver::checkResponseState(const MessageHeader& /*_rmsghdr*/, MessageId& /*_rrelay_id*/, const bool /*_erase_request*/) const
{
    return ResponseStateE::None;
}
/*virtual*/ void MessageReaderReceiver::pushCancelRequest(const RequestId&) {}
/*virtual*/ void MessageReaderReceiver::cancelRelayed(const MessageId&) {}
//-----------------------------------------------------------------------------

} // namespace mprpc
} // namespace frame
} // namespace solid
