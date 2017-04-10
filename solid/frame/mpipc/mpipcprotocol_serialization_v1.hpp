// solid/frame/mpipc/mpipcprotocol_binary_serialization_v1.hpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#ifndef SOLID_FRAME_MPIPC_MPIPCPROTOCOL_SERIALIZATION_V1_HPP
#define SOLID_FRAME_MPIPC_MPIPCPROTOCOL_SERIALIZATION_V1_HPP

#include "solid/frame/mpipc/mpipcprotocol.hpp"
#include "solid/serialization/binary.hpp"
#include "solid/serialization/binarybasic.hpp"
#include "solid/serialization/typeidmap.hpp"

namespace solid {
namespace frame {
namespace mpipc {
namespace serialization_v1 {

using SerializerT   = serialization::binary::Serializer<ConnectionContext>;
using DeserializerT = serialization::binary::Deserializer<ConnectionContext>;
using TypeIdMapT    = serialization::TypeIdMap<SerializerT, DeserializerT, TypeStub>;
using LimitsT       = serialization::binary::Limits;

struct Serializer : public mpipc::Serializer {
    SerializerT ser;

    Serializer(const LimitsT& _rlimits, TypeIdMapT const& _ridmap)
        : ser(_rlimits, &_ridmap)
    {
    }
    
    /*virtual*/ void push(MessageHeader &_rmsghdr) override{
        ser.push(_rmsghdr, "message_header");
    }

    /*virtual*/ void push(MessagePointerT& _rmsgptr, const size_t _msg_type_idx) override
    {
        ser.push(_rmsgptr, _msg_type_idx, "message");
    }
    /*virtual*/ int run(ConnectionContext& _rctx, char* _pdata, size_t _data_len) override
    {
        return ser.run(_pdata, _data_len, _rctx);
    }
    /*virtual*/ ErrorConditionT error() const override
    {
        return ser.error();
    }

    /*virtual*/ bool empty() const override
    {
        return ser.empty();
    }

    /*virtual*/ void clear() override
    {
        return ser.clear();
    }
};

struct Deserializer : public mpipc::Deserializer {
    DeserializerT des;

    Deserializer(const LimitsT& _rlimits, TypeIdMapT const& _ridmap)
        : des(_rlimits, &_ridmap)
    {
    }

    /*virtual*/ void push(MessageHeader &_rmsghdr) override{
        des.push(_rmsghdr, "message_header");
    }
    
    /*virtual*/ void push(MessagePointerT& _rmsgptr) override
    {
        des.push(_rmsgptr, "message");
    }
    /*virtual*/ int run(ConnectionContext& _rctx, const char* _pdata, size_t _data_len) override
    {
        return des.run(_pdata, _data_len, _rctx);
    }
    /*virtual*/ ErrorConditionT error() const override
    {
        return des.error();
    }

    /*virtual*/ bool empty() const override
    {
        return des.empty();
    }
    /*virtual*/ void clear() override
    {
        return des.clear();
    }
};

struct Protocol;
using ProtocolPointerT = std::shared_ptr<Protocol>;

struct Protocol : public mpipc::Protocol, std::enable_shared_from_this<Protocol> {
    TypeIdMapT type_map;

    LimitsT limits;

    static ProtocolPointerT create()
    {
        struct EnableMakeSharedProtocol : Protocol {
        };
        return std::make_shared<EnableMakeSharedProtocol>();
    }

    static ProtocolPointerT create(const LimitsT& _limits)
    {
        struct EnableMakeSharedProtocol : Protocol {
            EnableMakeSharedProtocol(const LimitsT& _limits)
                : Protocol(_limits)
            {
            }
        };
        return std::make_shared<EnableMakeSharedProtocol>(_limits);
    }

    ProtocolPointerT pointerFromThis()
    {
        return shared_from_this();
    }

    template <class T>
    size_t registerType(
        const size_t _protocol_id = 0,
        const size_t _idx         = 0)
    {
        TypeStub ts;
        size_t   rv = type_map.registerType<T>(ts, _protocol_id, _idx);
        return rv;
    }

    template <class T, class Allocator>
    size_t registerTypeAlloc(
        Allocator    _allocator,
        const size_t _protocol_id,
        const size_t _idx = 0)
    {
        TypeStub ts;
        size_t   rv = type_map.registerTypeAlloc<T>(_allocator, ts, _protocol_id, _idx);
        registerCast<T, mpipc::Message>();
        return rv;
    }

    template <class Msg, class CompleteFnc>
    size_t registerType(
        CompleteFnc  _complete_fnc,
        const size_t _protocol_id = 0,
        const size_t _idx         = 0)
    {
        TypeStub ts;

        using CompleteHandlerT = CompleteHandler<CompleteFnc,
            typename message_complete_traits<decltype(_complete_fnc)>::send_type,
            typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

        ts.complete_fnc = MessageCompleteFunctionT(CompleteHandlerT(_complete_fnc));

        size_t rv = type_map.registerType<Msg>(
            ts, Message::solidSerialize<SerializerT, Msg>, Message::solidSerialize<DeserializerT, Msg>,
            _protocol_id, _idx);
        registerCast<Msg, mpipc::Message>();
        return rv;
    }

    template <class Msg, class Allocator, class CompleteFnc>
    size_t registerTypeAlloc(
        Allocator    _allocator,
        CompleteFnc  _complete_fnc,
        const size_t _protocol_id = 0,
        const size_t _idx         = 0)
    {
        TypeStub ts;

        using CompleteHandlerT = CompleteHandler<CompleteFnc,
            typename message_complete_traits<decltype(_complete_fnc)>::send_type,
            typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

        ts.complete_fnc = MessageCompleteFunctionT(CompleteHandlerT(_complete_fnc));

        size_t rv = type_map.registerTypeAlloc<Msg>(
            ts, Message::solidSerialize<SerializerT, Msg>, Message::solidSerialize<DeserializerT, Msg>, _allocator,
            _protocol_id, _idx);
        registerCast<Msg, mpipc::Message>();
        return rv;
    }

    template <class Derived, class Base>
    bool registerCast()
    {
        return type_map.registerCast<Derived, Base>();
    }

    template <class Derived, class Base>
    bool registerDownCast()
    {
        return type_map.registerDownCast<Derived, Base>();
    }

    /*virtual*/ char* storeValue(char* _pd, uint8_t _v) const override
    {
        return serialization::binary::store(_pd, _v);
    }
    /*virtual*/ char* storeValue(char* _pd, uint16_t _v) const override
    {
        return serialization::binary::store(_pd, _v);
    }
    /*virtual*/ char* storeValue(char* _pd, uint32_t _v) const override
    {
        return serialization::binary::store(_pd, _v);
    }
    /*virtual*/ char* storeValue(char* _pd, uint64_t _v) const override
    {
        return serialization::binary::store(_pd, _v);
    }

    /*virtual*/ const char* loadValue(const char* _ps, uint8_t& _v) const override
    {
        return serialization::binary::load(_ps, _v);
    }
    /*virtual*/ const char* loadValue(const char* _ps, uint16_t& _v) const override
    {
        return serialization::binary::load(_ps, _v);
    }
    /*virtual*/ const char* loadValue(const char* _ps, uint32_t& _v) const override
    {
        return serialization::binary::load(_ps, _v);
    }
    /*virtual*/ const char* loadValue(const char* _ps, uint64_t& _v) const override
    {
        return serialization::binary::load(_ps, _v);
    }

    /*virtual*/ size_t typeIndex(const Message* _pmsg) const override
    {
        return type_map.index(_pmsg);
    }

    /*virtual*/ const TypeStub& operator[](const size_t _idx) const override
    {
        return type_map[_idx];
    }

    /*virtual*/ SerializerPointerT createSerializer() const override
    {
        return SerializerPointerT(new Serializer(limits, type_map));
    }
    /*virtual*/ DeserializerPointerT createDeserializer() const override
    {
        return DeserializerPointerT(new Deserializer(limits, type_map));
    }

    /*virtual*/ void reset(mpipc::Serializer& _ser) const override
    {
        static_cast<Serializer&>(_ser).ser.resetLimits();
    }
    /*virtual*/ void reset(mpipc::Deserializer& _des) const override
    {
        static_cast<Deserializer&>(_des).des.resetLimits();
    }

    /*virtual*/ size_t minimumFreePacketDataSize() const override
    {
        return 16;
    }

protected:
    Protocol() {}
    Protocol(const LimitsT& _limits)
        : limits(_limits)
    {
    }
};

template <class T>
void proto_spec_setup_helper(Protocol& _rproto, const size_t _protocol_id)
{
    T t;
    t(_rproto, _protocol_id, 0);
}

template <class T1, class T2, class... Args>
void proto_spec_setup_helper(Protocol& _rproto, const size_t _protocol_id)
{
    T1 t;
    t(_rproto, _protocol_id, 0);
    proto_spec_setup_helper<T2, Args...>(_rproto, _protocol_id);
}

template <class Arg, class T>
void proto_spec_setup_helper1(Protocol& _rproto, const size_t _protocol_id, Arg&& _uarg)
{
    T t(std::forward<Arg>(_uarg));
    t(_rproto, _protocol_id, 0);
}

template <class Arg, class T1, class T2, class... Args>
void proto_spec_setup_helper1(Protocol& _rproto, const size_t _protocol_id, Arg&& _uarg)
{
    T1 t(std::forward<Arg>(_uarg));
    t(_rproto, _protocol_id, 0);
    proto_spec_setup_helper1<Arg, T2, Args...>(_rproto, _protocol_id, std::forward<Arg>(_uarg));
}

template <size_t Idx, class... Args>
struct ProtoSpec {

    template <template <typename> class MessageSetup>
    static void setup(Protocol& _rproto)
    {
        proto_spec_setup_helper<MessageSetup<Args>...>(_rproto, Idx);
    }

    template <template <typename> class MessageSetup, class Arg>
    static void setup(Protocol& _rproto, const size_t _protocol_id, Arg&& _uarg)
    {
        proto_spec_setup_helper1<Arg, MessageSetup<Args>...>(_rproto, Idx, std::forward<Arg>(_uarg));
    }
};

} //namespace serialization_v1
} //namespace mpipc
} //namespace frame
} //namespace solid

#endif
