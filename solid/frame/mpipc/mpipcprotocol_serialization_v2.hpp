// solid/frame/mpipc/mpipcprotocol_binary_serialization_v2.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/mpipc/mpipcprotocol.hpp"
#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/serialization/v2/serialization.hpp"
#include "solid/system/error.hpp"
#include <memory>

namespace solid {
namespace frame {
namespace mpipc {
namespace serialization_v2 {

template <typename TypeId, class Ctx>
using SerializerTT = serialization::binary::Serializer<TypeId, Ctx>;
template <typename TypeId, class Ctx>
using DeserializerTT = serialization::binary::Deserializer<TypeId, Ctx>;
template <typename TypeId, typename Data>
using TypeMapTT = serialization::TypeMap<TypeId, ConnectionContext, SerializerTT, DeserializerTT, Data>;
using LimitsT   = serialization::binary::Limits;

template <class S>
class Serializer : public mpipc::Serializer {
    using SerializerT = S;
    SerializerT ser_;

public:
    template <class TM>
    Serializer(const TM& _rtm, const LimitsT& _rlimits)
        : ser_(_rtm.createSerializer(_rlimits))
    {
    }
    void limits(const LimitsT& _rl)
    {
        ser_.limits(_rl);
    }

private:
    long run(ConnectionContext& _rctx, char* _pdata, size_t _data_len, MessageHeader& _rmsghdr) override
    {
        return ser_.run(_pdata, static_cast<unsigned>(_data_len), [&_rmsghdr](SerializerT& _rs, ConnectionContext& _rctx) { _rs.add(_rmsghdr, _rctx, "message"); }, _rctx);
    }

    long run(ConnectionContext& _rctx, char* _pdata, size_t _data_len, MessagePointerT& _rmsgptr, const size_t /*_msg_type_idx*/) override
    {
        return ser_.run(_pdata, static_cast<unsigned>(_data_len), [&_rmsgptr](SerializerT& _rs, ConnectionContext& _rctx) { _rs.add(_rmsgptr, _rctx, "message"); }, _rctx);
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

template <class D>
class Deserializer : public mpipc::Deserializer {
    using DeserializerT = D;
    DeserializerT des_;

public:
    template <class TM>
    Deserializer(const TM& _rtm, const LimitsT& _rlimits)
        : des_(_rtm.createDeserializer(_rlimits))
    {
    }
    void limits(const LimitsT& _rl)
    {
        des_.limits(_rl);
    }

private:
    long run(ConnectionContext& _rctx, const char* _pdata, size_t _data_len, MessageHeader& _rmsghdr) override
    {
        return des_.run(_pdata, static_cast<unsigned>(_data_len), [&_rmsghdr](DeserializerT& _rd, ConnectionContext& _rctx) mutable { _rd.add(_rmsghdr, _rctx, "message"); }, _rctx);
    }

    long run(ConnectionContext& _rctx, const char* _pdata, size_t _data_len, MessagePointerT& _rmsgptr) override
    {
        return des_.run(_pdata, static_cast<unsigned>(_data_len), [&_rmsgptr](DeserializerT& _rd, ConnectionContext& _rctx) { _rd.add(_rmsgptr, _rctx, "message"); }, _rctx);
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
    struct TypeData {
        template <class F>
        TypeData(F&& _f)
            : complete_fnc_(std::forward<F>(_f))
        {
        }

        TypeData() {}

        void complete(ConnectionContext& _rctx, MessagePointerT& _p1, MessagePointerT& _p2, ErrorConditionT const& _e) const
        {
            complete_fnc_(_rctx, _p1, _p2, _e);
        }

        MessageCompleteFunctionT complete_fnc_;
    };

    using ThisT         = Protocol<TypeId>;
    using PointerT      = std::shared_ptr<ThisT>;
    using TypeIdT       = TypeId;
    using TypeMapT      = TypeMapTT<TypeIdT, TypeData>;
    using SerializerT   = Serializer<typename TypeMapT::SerializerT>;
    using DeserializerT = Deserializer<typename TypeMapT::DeserializerT>;

    TypeMapT type_map_;

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

    void null(const TypeId& _rtid)
    {
        type_map_.null(_rtid);
    }

    template <class T>
    size_t registerType(const TypeId& _rtid)
    {
        return type_map_.template registerType<T>(_rtid);
    }

    template <class T, class Allocator>
    size_t registerType(
        Allocator     _allocator,
        const TypeId& _rtid)
    {
        return type_map_.template registerType<T>(_allocator, _rtid);
    }

    template <class Msg, class CompleteFnc>
    size_t registerMessage(
        CompleteFnc   _complete_fnc,
        const TypeId& _rtid)
    {
        using CompleteHandlerT = CompleteHandler<CompleteFnc,
            typename message_complete_traits<decltype(_complete_fnc)>::send_type,
            typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

        size_t rv = type_map_.template registerType<Msg>(
            CompleteHandlerT(_complete_fnc), Message::solidSerializeV2<typename TypeMapT::SerializerT, Msg>, Message::solidDeserializeV2<typename TypeMapT::DeserializerT, Msg>,
            _rtid);
        registerCast<Msg, mpipc::Message>();
        return rv;
    }

    template <class Msg, class Allocator, class CompleteFnc>
    size_t registerMessage(
        Allocator     _allocator,
        CompleteFnc   _complete_fnc,
        const TypeId& _rtid)
    {
        using CompleteHandlerT = CompleteHandler<CompleteFnc,
            typename message_complete_traits<decltype(_complete_fnc)>::send_type,
            typename message_complete_traits<decltype(_complete_fnc)>::recv_type>;

        size_t rv = type_map_.template registerType<Msg>(
            CompleteHandlerT(_complete_fnc), Message::solidSerializeV2<typename TypeMapT::SerializerT, Msg>, Message::solidDeserializeV2<typename TypeMapT::DeserializerT, Msg>, _allocator,
            _rtid);
        registerCast<Msg, mpipc::Message>();
        return rv;
    }

    template <class Derived, class Base>
    void registerCast()
    {
        type_map_.template registerCast<Derived, Base>();
    }

    template <class Derived, class Base>
    void registerDownCast()
    {
        type_map_.template registerDownCast<Derived, Base>();
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
        return type_map_.index(_pmsg);
    }

    void complete(const size_t _idx, ConnectionContext& _rctx, MessagePointerT& _rsent_msg_ptr, MessagePointerT& _rrecv_msg_ptr, ErrorConditionT const& _rerr) const override
    {
        type_map_[_idx].complete(_rctx, _rsent_msg_ptr, _rrecv_msg_ptr, _rerr);
    }

    typename SerializerT::PointerT createSerializer(const WriterConfiguration& _rconfig) const override
    {
        const LimitsT l(_rconfig.string_size_limit, _rconfig.container_size_limit, _rconfig.string_size_limit);
        return typename SerializerT::PointerT(new SerializerT(type_map_, l));
    }

    typename DeserializerT::PointerT createDeserializer(const ReaderConfiguration& _rconfig) const override
    {
        const LimitsT l(_rconfig.string_size_limit, _rconfig.container_size_limit, _rconfig.string_size_limit);
        return typename DeserializerT::PointerT(new DeserializerT(type_map_, l));
    }

    void reconfigure(mpipc::Deserializer& _rdes, const ReaderConfiguration& _rconfig) const override
    {
        const LimitsT l(_rconfig.string_size_limit, _rconfig.container_size_limit, _rconfig.string_size_limit);
        static_cast<DeserializerT&>(_rdes).limits(l);
    }

    void reconfigure(mpipc::Serializer& _rser, const WriterConfiguration& _rconfig) const override
    {
        const LimitsT l(_rconfig.string_size_limit, _rconfig.container_size_limit, _rconfig.string_size_limit);
        static_cast<SerializerT&>(_rser).limits(l);
    }

    size_t minimumFreePacketDataSize() const override
    {
        return 16;
    }

protected:
    Protocol() {}
};

#define SOLID_PROTOCOL_V2(ser, rthis, ctx, name)                                                         \
    template <class S>                                                                                   \
    void solidSerializeV2(S& _s, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name) const \
    {                                                                                                    \
        solidSerializeV2(_s, *this, _rctx, _name);                                                       \
    }                                                                                                    \
    template <class S>                                                                                   \
    void solidSerializeV2(S& _s, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name)       \
    {                                                                                                    \
        solidSerializeV2(_s, *this, _rctx, _name);                                                       \
    }                                                                                                    \
    template <class S, class T>                                                                          \
    static void solidSerializeV2(S& ser, T& rthis, solid::frame::mpipc::ConnectionContext& ctx, const char* name)

} //namespace serialization_v2
} //namespace mpipc
} //namespace frame
} //namespace solid
