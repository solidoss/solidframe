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
//using TypeIdMapT    = serialization::TypeIdMap<SerializerT, DeserializerT, TypeStub>;
using LimitsT       = serialization::binary::Limits;

class Serializer : public mpipc::Serializer {
    SerializerT ser_;
public:
    Serializer(const LimitsT &_rlimits)
        : ser_(_rlimits)
    {
    }
    void limits(const LimitsT &_rl){
        ser_.limits(_rl);
    }
private:
    long run(ConnectionContext& _rctx, char* _pdata, size_t _data_len, MessageHeader& _rmsghdr) override
    {
        return ser_.run(_pdata, static_cast<unsigned>(_data_len), [&_rmsghdr](SerializerT &_rs, ConnectionContext& _rctx){_rs.add(_rmsghdr, _rctx, "message");}, _rctx);
    }

    long run(ConnectionContext& _rctx, char* _pdata, size_t _data_len, MessagePointerT& _rmsgptr, const size_t _msg_type_idx) override
    {
        return ser_.run(_pdata, static_cast<unsigned>(_data_len), [&_rmsgptr](SerializerT &_rs, ConnectionContext& _rctx){_rs.add(_rmsgptr, _rctx, "message");}, _rctx);
    }

    long run(ConnectionContext& _rctx, char* _pdata, size_t _data_len) override
    {
        return ser_.SerializerBase::run(_pdata, static_cast<unsigned>(_data_len), &_rctx);
    }

    ErrorConditionT error() const override
    {
        return ser_.Base::error();
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
    Deserializer(const LimitsT &_rlimits)
        : des_(_rlimits)
    {
    }
    void limits(const LimitsT &_rl){
        des_.limits(_rl);
    }
private:
    long run(ConnectionContext& _rctx, const char* _pdata, size_t _data_len, MessageHeader& _rmsghdr) override
    {
        return des_.run(_pdata, static_cast<unsigned>(_data_len), [&_rmsghdr](DeserializerT &_rd, ConnectionContext& _rctx){_rd.add(_rmsghdr, _rctx, "message");}, _rctx);
    }

    long run(ConnectionContext& _rctx, const char* _pdata, size_t _data_len, MessagePointerT& _rmsgptr) override
    {
        return des_.run(_pdata, static_cast<unsigned>(_data_len), [&_rmsgptr](DeserializerT &_rd, ConnectionContext& _rctx){_rd.add(_rmsgptr, _rctx, "message");}, _rctx);
    }
    long run(ConnectionContext& _rctx, const char* _pdata, size_t _data_len) override
    {
        return des_.DeserializerBase::run(_pdata, static_cast<unsigned>(_data_len), &_rctx);
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
    using TypeIdT = TypeId;

    //TypeIdMapT type_map_;

    static PointerT create()
    {
        struct EnableMakeSharedProtocol : ThisT {
        };
        return std::make_shared<EnableMakeSharedProtocol>();
    }

    PointerT pointerFromThis()
    {
        return this->shared_from_this();
    }
    
    void null(const TypeId &_rtid){
        //TODO:
    }
    
    template <class T>
    size_t registerType(const TypeId &_rtid)
    {
        //return type_map_.registerType<T>(_rtid);
        return 0;
    }

    template <class T, class Allocator>
    size_t registerType(
        Allocator    _allocator,
        const TypeId &_rtid)
    {
        //return type_map.registerType<T>(_allocator, _rtid);
        return 0;
    }

    template <class Msg, class CompleteFnc>
    size_t registerMessage(
        CompleteFnc  _complete_fnc,
        const TypeId &_rtid)
    {
        using CompleteHandlerT = CompleteHandler<CompleteFnc,
            typename message_complete_traits<decltype(_complete_fnc)>::send_type,
            typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

        //ts.complete_fnc = MessageCompleteFunctionT(CompleteHandlerT(_complete_fnc));

        //size_t rv = type_map.registerType<Msg>(
            //ts, Message::solidSerializeV1<SerializerT, Msg>, Message::solidSerializeV1<DeserializerT, Msg>,
           // _protocol_id, _idx);
        registerCast<Msg, mpipc::Message>();
        //return rv;
        return 0;
    }

    template <class Msg, class Allocator, class CompleteFnc>
    size_t registerMessage(
        Allocator    _allocator,
        CompleteFnc  _complete_fnc,
        const TypeId &_rtid)
    {
        //TypeStub ts;

        using CompleteHandlerT = CompleteHandler<CompleteFnc,
            typename message_complete_traits<decltype(_complete_fnc)>::send_type,
            typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

        //ts.complete_fnc = MessageCompleteFunctionT(CompleteHandlerT(_complete_fnc));

        //size_t rv = type_map.registerTypeAlloc<Msg>(
        //    ts, Message::solidSerializeV1<SerializerT, Msg>, Message::solidSerializeV1<DeserializerT, Msg>, _allocator,
        //    _protocol_id, _idx);
        registerCast<Msg, mpipc::Message>();
        //return rv;
        return 0;
    }

    template <class Derived, class Base>
    bool registerCast()
    {
        //return type_map_.registerCast<Derived, Base>();
        return false;
    }

    template <class Derived, class Base>
    bool registerDownCast()
    {
        //return type_map_.registerDownCast<Derived, Base>();
        return false;
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
        return serialization::binary::cross::store_with_check(_pd, _sz, _v);
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
        return serialization::binary::cross::load_with_check(_ps, _sz, _v);
    }
    
    size_t typeIndex(const Message* _pmsg) const override
    {
        //return type_map.index(_pmsg);
        return 0;
    }

    void complete(const size_t _idx, ConnectionContext& _rctx, MessagePointerT& _rsent_msg_ptr, MessagePointerT& _rrecv_msg_ptr, ErrorConditionT const& _rerr) const override
    {
        //type_map[_idx].complete_fnc(_rctx, _rsent_msg_ptr, _rrecv_msg_ptr, _rerr);
    }

    Serializer::PointerT createSerializer(const WriterConfiguration& _rconfig) const override
    {
        const LimitsT l(_rconfig.string_size_limit, _rconfig.container_size_limit, _rconfig.string_size_limit);
        return Serializer::PointerT(new Serializer(l));
    }

    Deserializer::PointerT createDeserializer(const ReaderConfiguration& _rconfig) const override
    {
        const LimitsT l(_rconfig.string_size_limit, _rconfig.container_size_limit, _rconfig.string_size_limit);
        return Deserializer::PointerT(new Deserializer(l));
    }

    void reconfigure(mpipc::Deserializer& _rdes, const ReaderConfiguration& _rconfig) const override
    {
        const LimitsT l(_rconfig.string_size_limit, _rconfig.container_size_limit, _rconfig.string_size_limit);
        static_cast<Deserializer&>(_rdes).limits(l);
    }

    void reconfigure(mpipc::Serializer& _rser, const WriterConfiguration& _rconfig) const override
    {
        const LimitsT l(_rconfig.string_size_limit, _rconfig.container_size_limit, _rconfig.string_size_limit);
        static_cast<Serializer&>(_rser).limits(l);
    }

    size_t minimumFreePacketDataSize() const override
    {
        return 16;
    }

protected:
    Protocol() {}
};

} //namespace serialization_v2
} //namespace mpipc
} //namespace frame
} //namespace solid

