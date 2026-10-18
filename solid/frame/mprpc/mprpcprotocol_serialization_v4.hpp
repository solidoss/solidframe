// solid/frame/mprpc/mprpcprotocol_serialization_v4.hpp
//
// Copyright (c) 2026 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol.hpp"
#include "solid/reflection/v2/reflection.hpp"
#include "solid/reflection/v2/typemap.hpp"
#include "solid/serialization/v4/serialization.hpp"
#include "solid/system/error.hpp"

// mprpc protocol over binary serialization v4 (reflection v2): messages are
// reflected with solidReflectV2. The wire format is the one of v3, so peers
// using either protocol interoperate.
namespace solid::frame::mprpc::serialization_v4 {

template <typename TypeId>
using SerializerTT = serialization::v4::binary::Serializer<ConnectionContext, TypeId>;
template <typename TypeId>
using DeserializerTT = serialization::v4::binary::Deserializer<ConnectionContext, TypeId>;

using TypeMapBaseT = solid::reflection::v2::TypeMapBase;

template <typename TypeId>
class Serializer : public mprpc::Serializer {
    using SerializerT = SerializerTT<TypeId>;
    SerializerT ser_;

public:
    explicit Serializer(const TypeMapBaseT& _rtm)
        : ser_(_rtm)
    {
    }

private:
    ptrdiff_t run(ConnectionContext& _rctx, char* _pdata, size_t _data_len, MessageHeader const& _rmsghdr) override
    {
        return ser_.run(
            _pdata, _data_len, [&_rmsghdr](SerializerT& _rs, ConnectionContext& _rctx) { _rs.add(_rmsghdr, _rctx); }, _rctx);
    }

    ptrdiff_t run(ConnectionContext& _rctx, char* _pdata, size_t _data_len, SendMessagePointerT<>& _rmsgptr, const size_t /*_msg_type_idx*/) override
    {
        return ser_.run(
            _pdata, _data_len, [&_rmsgptr](SerializerT& _rs, ConnectionContext& _rctx) { _rs.add(_rmsgptr, _rctx); }, _rctx);
    }

    ptrdiff_t run(ConnectionContext& _rctx, char* _pdata, size_t _data_len) override
    {
        return ser_.run(_pdata, _data_len, _rctx);
    }

    [[nodiscard]] ErrorConditionT error() const override
    {
        return ser_.error();
    }

    [[nodiscard]] bool empty() const override
    {
        return ser_.empty();
    }

    void clear() override
    {
        ser_.clear();
    }
};

template <typename TypeId>
class Deserializer : public mprpc::Deserializer {
    using DeserializerT = DeserializerTT<TypeId>;
    DeserializerT des_;

public:
    explicit Deserializer(const TypeMapBaseT& _rtm)
        : des_(_rtm)
    {
    }

private:
    ptrdiff_t run(ConnectionContext& _rctx, const char* _pdata, size_t _data_len, MessageHeader& _rmsghdr) override
    {
        return des_.run(
            _pdata, _data_len, [&_rmsghdr](DeserializerT& _rd, ConnectionContext& _rctx) { _rd.add(_rmsghdr, _rctx); }, _rctx);
    }

    ptrdiff_t run(ConnectionContext& _rctx, const char* _pdata, size_t _data_len, RecvMessagePointerT<>& _rmsgptr) override
    {
        return des_.run(
            _pdata, _data_len, [&_rmsgptr](DeserializerT& _rd, ConnectionContext& _rctx) { _rd.add(_rmsgptr, _rctx); }, _rctx);
    }

    ptrdiff_t run(ConnectionContext& _rctx, const char* _pdata, size_t _data_len) override
    {
        return des_.run(_pdata, _data_len, _rctx);
    }

    [[nodiscard]] ErrorConditionT error() const override
    {
        return des_.error();
    }

    [[nodiscard]] bool empty() const override
    {
        return des_.empty();
    }

    void clear() override
    {
        des_.clear();
    }
};

template <typename>
struct is_pair : std::false_type {
};

template <typename T1, typename T2>
struct is_pair<std::pair<T1, T2>> : std::true_type {
};

template <typename TypeId>
class Protocol : public mprpc::Protocol {
    struct TypeData {
        MessageCompleteFunctionT complete_fnc_;

        void complete(ConnectionContext& _rctx, SendMessagePointerT<>& _p1, RecvMessagePointerT<>& _p2, ErrorConditionT const& _e) const
        {
            complete_fnc_(_rctx, _p1, _p2, _e);
        }
    };

    using ThisT           = Protocol<TypeId>;
    using TypeMapT        = reflection::v2::TypeMap<SerializerTT<TypeId>, DeserializerTT<TypeId>>;
    using SerializerT     = Serializer<TypeId>;
    using DeserializerT   = Deserializer<TypeId>;
    using TypeDataVectorT = std::vector<TypeData>;

    TypeDataVectorT type_data_vec_;
    TypeMapT        type_map_;

    static reflection::v2::TypeCategory category(const TypeId& _id)
    {
        if constexpr (is_pair<TypeId>::value) {
            return reflection::v2::TypeCategory{static_cast<size_t>(_id.first)};
        } else {
            return reflection::v2::TypeCategory{};
        }
    }

    static reflection::v2::TypeId id(const TypeId& _id)
    {
        if constexpr (is_pair<TypeId>::value) {
            return reflection::v2::TypeId{static_cast<size_t>(_id.second)};
        } else {
            return reflection::v2::TypeId{static_cast<size_t>(_id)};
        }
    }

    // The message pointer the deserializer fills in is a MutableIntrusivePtr:
    // create the message there, with its header set from the connection.
    template <class CreateF>
    static auto message_creator(CreateF _create_fnc)
    {
        return [_create_fnc](ConnectionContext& _rctx, auto& _rptr) {
            using PtrT = std::decay_t<decltype(_rptr)>;
            if constexpr (solid::is_mutable_intrusive_ptr_v<PtrT>) {
                _create_fnc(_rctx, _rptr);
                if (_rptr) {
                    _rptr->header(_rctx);
                }
            }
        };
    }

    template <class TypeMapProxy>
    class Proxy {
        ThisT&        rproto_;
        TypeMapProxy& rmap_;

        template <class CompleteFnc>
        size_t storeComplete(const size_t _index, CompleteFnc&& _complete_fnc)
        {
            using CompleteFncT     = std::decay_t<CompleteFnc>;
            using CompleteHandlerT = CompleteHandler<CompleteFncT,
                typename message_complete_traits<CompleteFncT>::send_type,
                typename message_complete_traits<CompleteFncT>::recv_type>;
            if (_index >= rproto_.type_data_vec_.size()) {
                rproto_.type_data_vec_.resize(_index + 1);
            }
            rproto_.type_data_vec_[_index].complete_fnc_ = CompleteHandlerT(std::forward<CompleteFnc>(_complete_fnc));
            return _index;
        }

    public:
        Proxy(ThisT& _rproto, TypeMapProxy& _rmap)
            : rproto_(_rproto)
            , rmap_(_rmap)
        {
        }

        template <class T>
        size_t registerType(const TypeId _id, const std::string_view _name)
        {
            return rmap_.template register_type<T>(category(_id), id(_id), reflection::v2::TypeName{_name});
        }

        template <class T, class B>
        size_t registerType(const TypeId _id, const std::string_view _name)
        {
            return rmap_.template register_type<T, B>(category(_id), id(_id), reflection::v2::TypeName{_name});
        }

        template <class T, class B>
        void registerCast()
        {
            rmap_.template register_cast<T, B>();
        }

        template <class T, typename CompleteFnc>
        size_t registerMessage(const TypeId _id, const std::string_view _name, CompleteFnc _complete_fnc)
        {
            const size_t index = rmap_.template register_type<T, mprpc::Message>(
                category(_id), id(_id), reflection::v2::TypeName{_name},
                message_creator([](ConnectionContext&, MutableMessagePointerT<T>& _rptr) { _rptr = make_message<T>(); }));
            return storeComplete(index, std::move(_complete_fnc));
        }

        // _create_fnc(ConnectionContext&, MutableMessagePointerT<T>&) creates the message.
        template <class T, typename CompleteFnc, typename CreateFnc>
        size_t registerMessage(const TypeId _id, const std::string_view _name, CompleteFnc _complete_fnc, CreateFnc _create_fnc)
        {
            const size_t index = rmap_.template register_type<T, mprpc::Message>(
                category(_id), id(_id), reflection::v2::TypeName{_name}, message_creator(std::move(_create_fnc)));
            return storeComplete(index, std::move(_complete_fnc));
        }
    };

public:
    template <typename InitFnc>
    explicit Protocol(InitFnc _init_fnc)
        : type_map_(
              [this, &_init_fnc](auto& _rmap) {
                  Proxy<std::decay_t<decltype(_rmap)>> proxy(*this, _rmap);
                  _init_fnc(proxy);
              })
    {
    }

    char* storeValue(char* _pd, uint8_t _v) const override
    {
        return serialization::v4::binary::store(_pd, _v);
    }
    char* storeValue(char* _pd, uint16_t _v) const override
    {
        return serialization::v4::binary::store(_pd, _v);
    }
    char* storeValue(char* _pd, uint32_t _v) const override
    {
        return serialization::v4::binary::store(_pd, _v);
    }
    char* storeValue(char* _pd, uint64_t _v) const override
    {
        return serialization::v4::binary::store(_pd, _v);
    }

    char* storeCompactValue(char* _pd, const size_t _sz, uint32_t _v) const override
    {
        return serialization::v4::binary::store_compact(_pd, _sz, _v);
    }

    const char* loadValue(const char* _ps, uint8_t& _v) const override
    {
        return serialization::v4::binary::load(_ps, _v);
    }
    const char* loadValue(const char* _ps, uint16_t& _v) const override
    {
        return serialization::v4::binary::load(_ps, _v);
    }
    const char* loadValue(const char* _ps, uint32_t& _v) const override
    {
        return serialization::v4::binary::load(_ps, _v);
    }
    const char* loadValue(const char* _ps, uint64_t& _v) const override
    {
        return serialization::v4::binary::load(_ps, _v);
    }
    const char* loadCompactValue(const char* _ps, const size_t _sz, uint32_t& _v) const override
    {
        const char* p = serialization::v4::binary::load_compact(_ps, _sz, _v);
        return p == _ps ? nullptr : p; // invalid encoding: same as v3 - not loaded
    }

    size_t typeIndex(const Message* _pmsg) const override
    {
        return type_map_.index(_pmsg);
    }

    void complete(const size_t _idx, ConnectionContext& _rctx, SendMessagePointerT<>& _rsent_msg_ptr, RecvMessagePointerT<>& _rrecv_msg_ptr, ErrorConditionT const& _rerr) const override
    {
        type_data_vec_[_idx].complete(_rctx, _rsent_msg_ptr, _rrecv_msg_ptr, _rerr);
    }

    typename SerializerT::PointerT createSerializer(const WriterConfiguration& /*_rconfig*/) const override
    {
        return std::make_unique<SerializerT>(type_map_);
    }

    typename DeserializerT::PointerT createDeserializer(const ReaderConfiguration& /*_rconfig*/) const override
    {
        return std::make_unique<DeserializerT>(type_map_);
    }

    void reconfigure(mprpc::Deserializer& /*_rdes*/, const ReaderConfiguration& /*_rconfig*/) const override
    {
    }

    void reconfigure(mprpc::Serializer& /*_rser*/, const WriterConfiguration& /*_rconfig*/) const override
    {
    }

    [[nodiscard]] size_t minimumFreePacketDataSize() const override
    {
        return 16;
    }
};

// TypeId: the wire type of the message type ids - an integral (not bool) or a
// pair of integrals (category, id).
template <class TypeId, class InitFunction>
auto create_protocol(InitFunction _init_fnc)
{
    if constexpr (is_pair<TypeId>::value) {
        static_assert(std::is_integral_v<typename TypeId::first_type> && !std::is_same_v<typename TypeId::first_type, bool> && std::is_integral_v<typename TypeId::second_type> && !std::is_same_v<typename TypeId::second_type, bool>,
            "TypeId can only be either integral (but not bool) or pair of <integral (but not bool)>");
    } else {
        static_assert(std::is_integral_v<TypeId> && !std::is_same_v<TypeId, bool>, "TypeId can only be either integral (but not bool) or pair of <integral (but not bool)>");
    }

    return std::make_shared<Protocol<TypeId>>(_init_fnc);
}

} // namespace solid::frame::mprpc::serialization_v4
