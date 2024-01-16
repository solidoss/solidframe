// solid/frame/ipc/src/ipcmessagereader.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once
#include "mprpcutility.hpp"
#include "solid/frame/mprpc/mprpcprotocol.hpp"
#include "solid/system/common.hpp"
#include "solid/system/error.hpp"
#include <deque>

namespace solid {
namespace frame {
namespace mprpc {

struct ReaderConfiguration;

class PacketHeader;

struct MessageReaderReceiver {
    uint8_t                    request_buffer_ack_count_;
    ReaderConfiguration const& rconfig_;
    Protocol const&            rproto_;
    ConnectionContext&         rconctx_;
    const bool                 is_relay_enabled_ = false;

    MessageReaderReceiver(
        ReaderConfiguration const& _rconfig,
        Protocol const&            _rproto,
        ConnectionContext&         _rconctx,
        const bool                 _is_relay_enabled = false)
        : request_buffer_ack_count_(0)
        , rconfig_(_rconfig)
        , rproto_(_rproto)
        , rconctx_(_rconctx)
        , is_relay_enabled_(_is_relay_enabled)
    {
    }

    inline ReaderConfiguration const& configuration() const noexcept { return rconfig_; }
    inline Protocol const&            protocol() const noexcept { return rproto_; }
    inline ConnectionContext&         context() const noexcept { return rconctx_; }
    inline bool                       isRelayEnabled() const noexcept { return is_relay_enabled_; }

    virtual ~MessageReaderReceiver();

    virtual void           receiveMessage(MessagePointerT<>&, const size_t /*_msg_type_id*/) = 0;
    virtual void           receiveKeepAlive()                                                = 0;
    virtual void           receiveAckCount(uint8_t _count)                                   = 0;
    virtual void           receiveCancelRequest(const RequestId&)                            = 0;
    virtual bool           receiveRelayStart(MessageHeader& _rmsghdr, const char* _pbeg, size_t _sz, MessageId& _rrelay_id, const bool _is_last, ErrorConditionT& _rerror);
    virtual bool           receiveRelayBody(const char* _pbeg, size_t _sz, const MessageId& _rrelay_id, const bool _is_last, ErrorConditionT& _rerror);
    virtual bool           receiveRelayResponse(MessageHeader& _rmsghdr, const char* _pbeg, size_t _sz, const MessageId& _rrelay_id, const bool _is_last, ErrorConditionT& _rerror);
    virtual ResponseStateE checkResponseState(const MessageHeader& _rmsghdr, MessageId& _rrelay_id, const bool _erase_request = true) const;
    virtual void           pushCancelRequest(const RequestId&);
    virtual void           cancelRelayed(const MessageId&);
};

class MessageReader {
    enum struct StateE {
        ReadPacketHead = 1,
        ReadPacketBody,
    };

    struct MessageStub {
        enum struct StateE : uint8_t {
            NotStarted,
            ReadHeadStart,
            ReadHeadContinue,
            ReadBodyStart,
            ReadBodyContinue,
            IgnoreBody,
            RelayStart,
            RelayBody,
            RelayFail,
            RelayResponse,
        };

        MessagePointerT<>      message_ptr_;
        Deserializer::PointerT deserializer_ptr_;
        MessageHeader          message_header_;
        size_t                 packet_count_ = 0;
        MessageId              relay_id;
        StateE                 state_ = StateE::NotStarted;

        MessageStub() = default;

        void clear()
        {
            message_ptr_.reset();
            deserializer_ptr_.reset();
            packet_count_ = 0;
            state_        = StateE::NotStarted;
            relay_id.clear();
        }
    };
    using MessageVectorT = std::deque<MessageStub>;

    StateE                 state_ = StateE::ReadPacketHead;
    MessageVectorT         message_vec_;
    Deserializer::PointerT des_top_;

public:
    MessageReader() = default;

    ~MessageReader();

    size_t read(
        const char*            _pbuf,
        size_t                 _bufsz,
        MessageReaderReceiver& _receiver,
        ErrorConditionT&       _rerror);

    void prepare(ReaderConfiguration const& _rconfig);
    void unprepare();

private:
    void doConsumePacket(
        const char*            _pbuf,
        PacketHeader const&    _packet_header,
        MessageReaderReceiver& _receiver,
        ErrorConditionT&       _rerror);

    void doConsumePacketLoop(
        const char*            _pbufpos,
        const char* const      _pbufend,
        MessageReaderReceiver& _receiver,
        ErrorConditionT&       _rerror);

    const char* doConsumeMessage(
        const char*            _pbufpos,
        const char* const      _pbufend,
        const uint32_t         _msgidx,
        const uint8_t          _cmd,
        MessageReaderReceiver& _receiver,
        ErrorConditionT&       _rerror);

    bool doConsumeMessageHeader(
        const char*&           _pbufpos,
        const char* const      _pbufend,
        const uint32_t         _msgidx,
        uint16_t&              _message_size,
        MessageReaderReceiver& _receiver,
        ErrorConditionT&       _rerror);
    bool doConsumeMessageBody(
        const char*&           _pbufpos,
        const char* const      _pbufend,
        const uint32_t         _msgidx,
        const uint8_t          _cmd,
        uint16_t&              _message_size,
        MessageReaderReceiver& _receiver,
        ErrorConditionT&       _rerror);

    bool doConsumeMessageIgnoreBody(
        const char*&           _pbufpos,
        const char* const      _pbufend,
        const uint32_t         _msgidx,
        const uint8_t          _cmd,
        uint16_t&              _message_size,
        MessageReaderReceiver& _receiver);

    void doConsumeMessageRelayStart(
        const char*&           _pbufpos,
        const char* const      _pbufend,
        const uint32_t         _msgidx,
        const uint8_t          _cmd,
        uint16_t&              _message_size,
        MessageReaderReceiver& _receiver,
        ErrorConditionT&       _rerror);

    void doConsumeMessageRelayBody(
        const char*&           _pbufpos,
        const char* const      _pbufend,
        const uint32_t         _msgidx,
        const uint8_t          _cmd,
        uint16_t&              _message_size,
        MessageReaderReceiver& _receiver,
        ErrorConditionT&       _rerror);

    void doConsumeMessageRelayFail(
        const char*&           _pbufpos,
        const char* const      _pbufend,
        const uint32_t         _msgidx,
        const uint8_t          _cmd,
        uint16_t&              _message_size,
        MessageReaderReceiver& _receiver,
        ErrorConditionT&       _rerror);

    void doConsumeMessageRelayResponse(
        const char*&           _pbufpos,
        const char* const      _pbufend,
        const uint32_t         _msgidx,
        const uint8_t          _cmd,
        uint16_t&              _message_size,
        MessageReaderReceiver& _receiver,
        ErrorConditionT&       _rerror);

    void                   cache(Deserializer::PointerT& _des);
    Deserializer::PointerT createDeserializer(MessageReaderReceiver& _receiver);
};

} // namespace mprpc
} // namespace frame
} // namespace solid
