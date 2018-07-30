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

#include "solid/system/cassert.hpp"
#include "solid/system/log.hpp"
#include "solid/system/socketaddress.hpp"

#include "solid/frame/mpipc/mpipcservice.hpp"

namespace solid {
namespace frame {
namespace mpipc {

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

struct PacketHeader {
    enum {
        SizeOfE = 4,
    };

    enum struct TypeE : uint8_t {
        Data = 1,
        KeepAlive,
    };

    enum struct CommandE : uint8_t {
        EndMessageFlag = 1, //do not change the values
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
        Size64KB   = 1, // DO NOT CHANGE!!
        Compressed = 2,
        AckRequest = 4,
    };

    PacketHeader(
        const TypeE    _type  = TypeE::Data,
        const uint8_t  _flags = 0,
        const uint16_t _size  = 0)
    {
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

    uint32_t size() const
    {
        uint32_t sz = (flags_ & static_cast<uint32_t>(FlagE::Size64KB));
        return (sz << 16) | size_;
    }

    uint8_t type() const
    {
        return type_;
    }

    uint8_t flags() const
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

    void size(uint32_t _sz)
    {
        size_ = _sz & 0xffff;
        flags_ |= ((_sz & (1 << 16)) >> 16);
    }

    bool isTypeKeepAlive() const
    {
        return type_ == static_cast<uint8_t>(TypeE::KeepAlive);
    }

    bool isCompressed() const
    {
        return flags_ & static_cast<uint8_t>(FlagE::Compressed);
    }
    bool isOk() const;

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

private:
    uint8_t  type_;
    uint8_t  flags_;
    uint16_t size_;
};

struct MessageBundle {
    size_t                   message_type_id;
    MessageFlagsT            message_flags;
    MessagePointerT          message_ptr;
    MessageCompleteFunctionT complete_fnc;
    std::string              message_url;

    MessageBundle()
        : message_type_id(InvalidIndex())
        , message_flags(0)
    {
    }

    MessageBundle(
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        const MessageFlagsT&      _flags,
        MessageCompleteFunctionT& _complete_fnc,
        std::string&              _rmessage_url)
        : message_type_id(_msg_type_idx)
        , message_flags(_flags)
        , message_ptr(std::move(_rmsgptr))
        , message_url(std::move(_rmessage_url))
    {
        std::swap(complete_fnc, _complete_fnc);
    }

    MessageBundle(
        MessageBundle&& _rmsgbundle)
        : message_type_id(_rmsgbundle.message_type_id)
        , message_flags(_rmsgbundle.message_flags)
        , message_ptr(std::move(_rmsgbundle.message_ptr))
        , message_url(std::move(_rmsgbundle.message_url))
    {
        std::swap(complete_fnc, _rmsgbundle.complete_fnc);
    }

    MessageBundle& operator=(MessageBundle&& _rmsgbundle)
    {
        message_type_id = _rmsgbundle.message_type_id;
        message_flags   = _rmsgbundle.message_flags;
        message_ptr     = std::move(_rmsgbundle.message_ptr);
        message_url     = std::move(_rmsgbundle.message_url);
        solid_function_clear(complete_fnc);
        std::swap(complete_fnc, _rmsgbundle.complete_fnc);
        return *this;
    }

    void clear()
    {
        message_type_id = InvalidIndex();
        message_flags.reset();
        message_ptr.reset();
        message_url.clear();
        solid_function_clear(complete_fnc);
    }
};

} //namespace mpipc
} //namespace frame
} //namespace solid
