// solid/frame/mpipc/mpipcprotocol_binary_serialization_v1.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <memory>
#include "solid/system/error.hpp"
#include "solid/frame/mpipc/mpipcprotocol.hpp"
#include "solid/serialization/v2/serialization.hpp"

namespace solid {
namespace frame {
namespace mpipc {
namespace serialization_v2 {

using SerializerT   = serialization::binary::Serializer<ConnectionContext>;
using DeserializerT = serialization::binary::Deserializer<ConnectionContext>;
using TypeIdMapT    = serialization::TypeIdMap<SerializerT, DeserializerT, TypeStub>;
using LimitsT       = serialization::binary::Limits;

class Serializer : public mpipc::Serializer {
    SerializerT ser_;
public:
    Serializer(TypeIdMapT const& _ridmap)
        : ser_(&_ridmap)
    {
    }
private:
    long run(ConnectionContext& _rctx, char* _pdata, size_t _data_len, MessageHeader& _rmsghdr) override
    {
        ser_.push(_rmsghdr, "message_header");
        return ser_.run(_pdata, static_cast<unsigned>(_data_len), [&_rmsghdr](SerializerT &_rs, ConnectionContext& _rctx){_rs.add(_rmsghdr, _rctx, "message");}, _rctx);
    }

    long run(ConnectionContext& _rctx, char* _pdata, size_t _data_len, MessagePointerT& _rmsgptr, const size_t _msg_type_idx) override
    {
        return ser_.run(_pdata, static_cast<unsigned>(_data_len), [&_rmsgptr](SerializerT &_rs, ConnectionContext& _rctx){_rs.add(_rmsgptr, _rctx, "message");}, _rctx);
    }

    long run(ConnectionContext& _rctx, char* _pdata, size_t _data_len) override
    {
        return ser_.run(_pdata, static_cast<unsigned>(_data_len), _rctx);
    }

    ErrorConditionT error() const override
    {
        return ser_.error();
    }

    bool empty() const override
    {
        return ser_.empty();
    }

    void clear() override
    {
        return ser_.clear();
    }
};

class Deserializer : public mpipc::Deserializer {
    DeserializerT des_;
public:
    Deserializer(TypeIdMapT const& _ridmap)
        : des_(&_ridmap)
    {
    }
private:
    long run(ConnectionContext& _rctx, const char* _pdata, size_t _data_len, MessageHeader& _rmsghdr) override
    {
        des.push(_rmsghdr, "message_header");
        return des.run(_pdata, static_cast<unsigned>(_data_len), _rctx);
    }

    long run(ConnectionContext& _rctx, const char* _pdata, size_t _data_len, MessagePointerT& _rmsgptr) override
    {
        des.push(_rmsgptr, "message");
        return des.run(_pdata, static_cast<unsigned>(_data_len), _rctx);
    }
    long run(ConnectionContext& _rctx, const char* _pdata, size_t _data_len) override
    {
        return des_.run(_pdata, static_cast<unsigned>(_data_len), _rctx);
    }
    ErrorConditionT error() const override
    {
        return des_.error();
    }

    bool empty() const override
    {
        return des_.empty();
    }
    void clear() override
    {
        return des_.clear();
    }
};

template <typename TypeId>
struct Protocol : public mpipc::Protocol, std::enable_shared_from_this<Protocol<TypeId>> {
    using ThisT = Protocol<TypeId>;
    using PointerT = std::shared_ptr<ThisT>;

    TypeIdMapT type_map_;

    static PointerT create()
    {
        struct EnableMakeSharedProtocol : ThisT {
        };
        return std::make_shared<EnableMakeSharedProtocol>();
    }

    PointerT pointerFromThis()
    {
        return shared_from_this();
    }

    template <class T>
    size_t registerType(const TypeId &_rtid)
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
            ts, Message::solidSerializeV1<SerializerT, Msg>, Message::solidSerializeV1<DeserializerT, Msg>,
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
            ts, Message::solidSerializeV1<SerializerT, Msg>, Message::solidSerializeV1<DeserializerT, Msg>, _allocator,
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

    char* storeValue(char* _pd, uint8_t _v) const override
    {
        return serialization::binary::store(_pd, _v);
    }
    char* storeValue(char* _pd, uint16_t _v) const override
    {
        return serialization::binary::store(_pd, _v);
    }
    char* storeValue(char* _pd, uint32_t _v) const override
    {
        return serialization::binary::store(_pd, _v);
    }
    char* storeValue(char* _pd, uint64_t _v) const override
    {
        return serialization::binary::store(_pd, _v);
    }

    char* storeCrossValue(char* _pd, const size_t _sz, uint32_t _v) const override
    {
        return serialization::binary::cross::store(_pd, _sz, _v);
    }

    const char* loadValue(const char* _ps, uint8_t& _v) const override
    {
        return serialization::binary::load(_ps, _v);
    }
    const char* loadValue(const char* _ps, uint16_t& _v) const override
    {
        return serialization::binary::load(_ps, _v);
    }
    const char* loadValue(const char* _ps, uint32_t& _v) const override
    {
        return serialization::binary::load(_ps, _v);
    }
    const char* loadValue(const char* _ps, uint64_t& _v) const override
    {
        return serialization::binary::load(_ps, _v);
    }
    const char* loadCrossValue(const char* _ps, const size_t _sz, uint32_t& _v) const override
    {
        return serialization::binary::cross::load(_ps, _sz, _v);
    }
    
    size_t typeIndex(const Message* _pmsg) const override
    {
        return type_map.index(_pmsg);
    }

    void complete(const size_t _idx, ConnectionContext& _rctx, MessagePointerT& _rsent_msg_ptr, MessagePointerT& _rrecv_msg_ptr, ErrorConditionT const& _rerr) const override
    {
        type_map[_idx].complete_fnc(_rctx, _rsent_msg_ptr, _rrecv_msg_ptr, _rerr);
    }

    Serializer::PointerT createSerializer(const WriterConfiguration& _rconfig) const override
    {
        return Serializer::PointerT(new Serializer(limits, type_map));
    }

    Deserializer::PointerT createDeserializer(const ReaderConfiguration& _rconfig) const override
    {
        return Deserializer::PointerT(new Deserializer(limits, type_map));
    }

    void reconfigure(mpipc::Deserializer& _rdes, const ReaderConfiguration& _rconfig) const override
    {
        static_cast<Deserializer&>(_rdes).des.resetLimits();
    }

    void reconfigure(mpipc::Serializer& _rser, const WriterConfiguration& _rconfig) const override
    {
        static_cast<Serializer&>(_rser).ser.resetLimits();
    }

    size_t minimumFreePacketDataSize() const override
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

} //namespace serialization_v2
} //namespace mpipc
} //namespace frame
} //namespace solid

