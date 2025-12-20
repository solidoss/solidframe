// solid/frame/ipc/src/ipcutility.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/mprpc/mprpcprotocol.hpp"
#include "solid/system/cassert.hpp"
#include "solid/system/log.hpp"
#include "solid/system/socketaddress.hpp"

#include "solid/frame/mprpc/mprpcservice.hpp"
#include <limits>

namespace solid::frame::mprpc {

enum struct ResponseStateE {
    Wait,
    RelayedWait,
    Cancel,
    Invalid,
    None
};

struct SocketAddressHash {
    size_t operator()(const SocketAddressInet* const& _rsa) const
    {
        return _rsa->hash();
    }
};

struct SocketAddressEqual {
    bool operator()(
        const SocketAddressInet* const& _rsa1,
        const SocketAddressInet* const& _rsa2) const
    {
        return *_rsa1 == *_rsa2;
    }
};

class PacketHeader {
    uint8_t  type_;
    uint8_t  flags_;
    uint16_t size_;

public:
    static constexpr size_t   header_size = 4;
    static constexpr uint16_t max_size    = Protocol::max_packet_size - header_size;

    enum struct TypeE : uint8_t {
        Data = 1,
        KeepAlive,
    };

    enum struct CommandE : uint8_t {
        EndMessageFlag = 1, // do not change the values
        NewMessage     = 2,
        FullMessage    = 3,
        Message        = 4,
        EndMessage     = 5,
        CancelMessage  = 6,
        Update         = 7,
        CancelRequest  = 8,
        AckdCount      = 9,

    };

    enum struct FlagE : uint8_t {
        Compressed = 2,
        AckRequest = 4,
    };

    PacketHeader(
        const TypeE    _type  = TypeE::Data,
        const uint8_t  _flags = 0,
        const uint16_t _size  = 0)
    {
        static_assert(Protocol::packet_header_size == header_size);
        reset(_type, _flags, _size);
    }

    void reset(
        const TypeE    _type  = TypeE::Data,
        const uint8_t  _flags = 0,
        const uint16_t _size  = 0)
    {
        type(_type);
        flags(_flags);
        size(_size);
    }

    [[nodiscard]] uint16_t size() const
    {
        return size_;
    }

    [[nodiscard]] uint8_t type() const
    {
        return type_;
    }

    [[nodiscard]] uint8_t flags() const
    {
        return flags_;
    }

    void type(const TypeE _type)
    {
        type_ = static_cast<uint8_t>(_type);
    }
    void flags(uint8_t _flags)
    {
        flags_ = _flags /*& (0xff - Size64KBFlagE)*/;
    }

    void size(uint16_t const _sz)
    {
        size_ = _sz;
    }

    [[nodiscard]] bool isTypeKeepAlive() const
    {
        return type_ == static_cast<uint8_t>(TypeE::KeepAlive);
    }

    [[nodiscard]] bool isCompressed() const
    {
        return (flags_ & static_cast<uint8_t>(FlagE::Compressed)) != 0U;
    }
    [[nodiscard]] bool isOk() const
    {
        switch (static_cast<TypeE>(type_)) {
        case TypeE::Data:
        case TypeE::KeepAlive:
            break;
        default:
            return false;
        }

        return size() <= max_size;
    }

    char* store(char* _pc, const Protocol& _rproto) const
    {
        _pc = _rproto.storeValue(_pc, type_);
        _pc = _rproto.storeValue(_pc, flags_);
        _pc = _rproto.storeValue(_pc, size_);
        return _pc;
    }

    const char* load(const char* _pc, const Protocol& _rproto)
    {
        _pc = _rproto.loadValue(_pc, type_);
        _pc = _rproto.loadValue(_pc, flags_);
        _pc = _rproto.loadValue(_pc, size_);
        return _pc;
    }
};

struct MessageBundle {
    size_t                      message_type_id = InvalidIndex();
    MessageFlagsT               message_flags   = 0;
    SendMessagePointerT<>       message_ptr;
    MessageCompleteFunctionT    complete_fnc;
    OptionalMessageRelayHeaderT message_relay_header_;

    MessageBundle() = default;

    MessageBundle(
        SendMessagePointerT<>&&       _rmsgptr,
        const size_t                  _msg_type_idx,
        const MessageFlagsT&          _flags,
        MessageCompleteFunctionT&     _complete_fnc,
        OptionalMessageRelayHeaderT&& _relay)
        : message_type_id(_msg_type_idx)
        , message_flags(_flags)
        , message_ptr(std::move(_rmsgptr))
        , message_relay_header_(std::move(_relay))
    {
        std::swap(complete_fnc, _complete_fnc);
    }

    MessageBundle(
        SendMessagePointerT<>&&            _rmsgptr,
        const size_t                       _msg_type_idx,
        const MessageFlagsT&               _flags,
        MessageCompleteFunctionT&          _complete_fnc,
        const OptionalMessageRelayHeaderT& _relay)
        : message_type_id(_msg_type_idx)
        , message_flags(_flags)
        , message_ptr(std::move(_rmsgptr))
        , message_relay_header_(_relay)
    {
        std::swap(complete_fnc, _complete_fnc);
    }

    MessageBundle(
        SendMessagePointerT<>&&   _rmsgptr,
        const size_t              _msg_type_idx,
        const MessageFlagsT&      _flags,
        MessageCompleteFunctionT& _complete_fnc)
        : message_type_id(_msg_type_idx)
        , message_flags(_flags)
        , message_ptr(std::move(_rmsgptr))
    {
        std::swap(complete_fnc, _complete_fnc);
    }

    MessageBundle(
        MessageBundle&& _rmsgbundle)
        : message_type_id(_rmsgbundle.message_type_id)
        , message_flags(_rmsgbundle.message_flags)
        , message_ptr(std::move(_rmsgbundle.message_ptr))
        , message_relay_header_(std::move(_rmsgbundle.message_relay_header_))
    {
        std::swap(complete_fnc, _rmsgbundle.complete_fnc);
    }

    MessageBundle& operator=(MessageBundle&& _rmsgbundle)
    {
        message_type_id       = _rmsgbundle.message_type_id;
        message_flags         = _rmsgbundle.message_flags;
        message_ptr           = std::move(_rmsgbundle.message_ptr);
        message_relay_header_ = std::move(_rmsgbundle.message_relay_header_);
        complete_fnc          = nullptr;
        std::swap(complete_fnc, _rmsgbundle.complete_fnc);
        return *this;
    }

    void clear()
    {
        message_type_id = InvalidIndex();
        message_flags.reset();
        message_ptr.reset();
        message_relay_header_.reset();
        complete_fnc = nullptr;
    }
};

} // namespace solid::frame::mprpc
