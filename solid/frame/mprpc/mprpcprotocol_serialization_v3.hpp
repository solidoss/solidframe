// solid/frame/mprpc/mprpcprotocol_binary_serialization_v3.hpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <string_view>
#include <memory>

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol.hpp"
#include "solid/reflection/v1/reflection.hpp"
#include "solid/serialization/v3/serialization.hpp"
#include "solid/system/error.hpp"

namespace solid {
namespace frame {
namespace mprpc {
namespace serialization_v3 {

    
template <class MetadataVariant, class MetadataFactory, typename TypeId>
using SerializerTT = serialization::v3::binary::Serializer<MetadataVariant, MetadataFactory, ConnectionContext, TypeId>;
template <class MetadataVariant, class MetadataFactory, typename TypeId>
using DeserializerTT = serialization::v3::binary::Deserializer<MetadataVariant, MetadataFactory, ConnectionContext, TypeId>;

using TypeMapBaseT = solid::reflection::v1::TypeMapBase;

template <class ...Reflector>
using TypeMapTT = solid::reflection::v1::TypeMap<Reflector...>;

template <class MetadataVariant, class MetadataFactory, typename TypeId>
class Serializer : public mprpc::Serializer {
    using SerializerT = SerializerTT<MetadataVariant, MetadataFactory, TypeId>;
    SerializerT ser_;

public:
    Serializer(MetadataFactory &&_meta_data_factory, const TypeMapBaseT& _rtm)
        : ser_(_meta_data_factory, _rtm)
    {
    }

private:
    long run(ConnectionContext& _rctx, char* _pdata, size_t _data_len, MessageHeader& _rmsghdr) override
    {
        return ser_.run(
            _pdata, static_cast<unsigned>(_data_len), [&_rmsghdr](SerializerT& _rs, ConnectionContext& _rctx) { _rs.add(_rmsghdr, _rctx, 0, "header"); }, _rctx);
    }

    long run(ConnectionContext& _rctx, char* _pdata, size_t _data_len, MessagePointerT& _rmsgptr, const size_t /*_msg_type_idx*/) override
    {
        return ser_.run(
            _pdata, static_cast<unsigned>(_data_len), [&_rmsgptr](SerializerT& _rs, ConnectionContext& _rctx) { _rs.add(_rmsgptr, _rctx, 0, "message"); }, _rctx);
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

template <class MetadataVariant, class MetadataFactory, typename TypeId>
class Deserializer : public mprpc::Deserializer {
    using DeserializerT = DeserializerTT<MetadataVariant, MetadataFactory, TypeId>;
    DeserializerT des_;

public:
    Deserializer(MetadataFactory &&_meta_data_factory, const TypeMapBaseT& _rtm)
        : des_(_meta_data_factory, _rtm)
    {
    }

private:
    long run(ConnectionContext& _rctx, const char* _pdata, size_t _data_len, MessageHeader& _rmsghdr) override
    {
        return des_.run(
            _pdata, static_cast<unsigned>(_data_len), [&_rmsghdr](DeserializerT& _rd, ConnectionContext& _rctx) mutable { _rd.add(_rmsghdr, _rctx, 0, "header"); }, _rctx);
    }

    long run(ConnectionContext& _rctx, const char* _pdata, size_t _data_len, MessagePointerT& _rmsgptr) override
    {
        return des_.run(
            _pdata, static_cast<unsigned>(_data_len), [&_rmsgptr](DeserializerT& _rd, ConnectionContext& _rctx) { _rd.add(_rmsgptr, _rctx, 0, "message"); }, _rctx);
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

template <class MetadataVariant, class MetadataFactory, typename TypeId, typename Hash = std::hash<TypeId>>
class Protocol : public mprpc::Protocol{
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

    using ThisT         = Protocol<MetadataVariant, MetadataFactory, TypeId, Hash>;
    using PointerT      = std::shared_ptr<ThisT>;
    using TypeIdT       = TypeId;
    using TypeMapT      = TypeMapTT<
        SerializerTT<MetadataVariant, MetadataFactory, TypeId>,
        DeserializerTT<MetadataVariant, MetadataFactory, TypeId>,
        reflection::v1::ReflectorT<MetadataVariant, MetadataFactory, ConnectionContext>,
        reflection::v1::ConstReflectorT<MetadataVariant, MetadataFactory, ConnectionContext>
    >;
    using SerializerT   = Serializer<MetadataVariant, MetadataFactory, TypeId>;
    using DeserializerT = Deserializer<MetadataVariant, MetadataFactory, TypeId>;
    using TypeDataVectorT = std::vector<TypeData>;
    
    MetadataFactory         metadata_factory_;
    TypeDataVectorT         type_data_vec_;
    TypeMapT                type_map_;
    
    template <class TypeMap>
    struct Proxy{
        ThisT    &rproto_;
        TypeMap  &rmap_;
        
        Proxy(ThisT &_rproto, TypeMap  &_rmap):rproto_(_rproto), rmap_(_rmap){}
        
        template <class T>
        size_t registerType(const size_t _category, const size_t _id, const std::string_view _name){
            return 0;
        }
        template <class T, class B>
        size_t registerType(const size_t _category, const size_t _id, const std::string_view _name){
            return 0;
        }
        template <class T, class B>
        void registerCast(){
        }
        template <class T, typename CompleteFnc>
        size_t registerMessage(const size_t _category, const size_t _id, const std::string_view _name, CompleteFnc _complete_fnc){
            using CompleteFncT     = CompleteFnc;
            using CompleteHandlerT = CompleteHandler<CompleteFncT,
                typename message_complete_traits<CompleteFncT>::send_type,
                typename message_complete_traits<CompleteFncT>::recv_type>;

            const size_t index = rmap_.template registerType<T, mprpc::Message>(_category, _id, _name);
            if(index >= rproto_.type_data_vec_.size()){
                rproto_.type_data_vec_.resize(index + 1);
            }
            rproto_.type_data_vec_[index].complete_fnc_ = CompleteHandlerT(std::move(_complete_fnc));
            return index;
        }
    };
public:
    template <typename InitFnc>
    Protocol(
        MetadataFactory &&_rmetadata_factory,
        InitFnc _init_fnc
    ):  metadata_factory_(std::forward<MetadataFactory>(_rmetadata_factory))
    ,   type_map_(
        [this, _init_fnc](auto &_rmap){
            //_rmap is typemap::Proxy
            Proxy<decltype(_rmap)>  proxy(*this, _rmap);
            _init_fnc(proxy);
        }
    )
    {}
    
#if 0
    void null(const TypeId& _rtid)
    {
        type_map_.null(_rtid);
    }

    template <class T>
    size_t registerType(const TypeId& _rtid)
    {
        return type_map_.registerType<T>(_rtid);
    }

    template <class T, class Allocator>
    size_t registerType(
        Allocator     _allocator,
        const TypeId& _rtid)
    {
        return type_map_.template registerType<T>(_allocator, _rtid);
    }

    template <class Msg, typename CompleteFnc>
    size_t registerMessage(
        CompleteFnc   _complete_fnc,
        const TypeId& _rtid)
    {
        using CompleteFncT     = CompleteFnc;
        using CompleteHandlerT = CompleteHandler<CompleteFncT,
            typename message_complete_traits<CompleteFncT>::send_type,
            typename message_complete_traits<CompleteFncT>::recv_type>;

        size_t rv = type_map_.template registerType<Msg>(
            CompleteHandlerT(std::move(_complete_fnc)), Message::solidSerializeV2<typename TypeMapT::SerializerT, Msg>, Message::solidDeserializeV2<typename TypeMapT::DeserializerT, Msg>,
            _rtid);
        registerCast<Msg, mprpc::Message>();
        return rv;
    }

    template <class Msg, class Allocator, class CompleteFnc>
    size_t registerMessage(
        Allocator     _allocator,
        CompleteFnc   _complete_fnc,
        const TypeId& _rtid)
    {
        using CompleteFncT     = CompleteFnc;
        using CompleteHandlerT = CompleteHandler<CompleteFnc,
            typename message_complete_traits<CompleteFncT>::send_type,
            typename message_complete_traits<CompleteFncT>::recv_type>;

        size_t rv = type_map_.template registerType<Msg>(
            CompleteHandlerT(std::move(_complete_fnc)), Message::solidSerializeV2<typename TypeMapT::SerializerT, Msg>, Message::solidDeserializeV2<typename TypeMapT::DeserializerT, Msg>, _allocator,
            _rtid);
        registerCast<Msg, mprpc::Message>();
        return rv;
    }

    template <class Derived, class Base>
    void registerCast()
    {
        type_map_.template registerCast<Derived, Base>();
    }
#endif

    char* storeValue(char* _pd, uint8_t _v) const override
    {
        return serialization::v3::binary::store(_pd, _v);
    }
    char* storeValue(char* _pd, uint16_t _v) const override
    {
        return serialization::v3::binary::store(_pd, _v);
    }
    char* storeValue(char* _pd, uint32_t _v) const override
    {
        return serialization::v3::binary::store(_pd, _v);
    }
    char* storeValue(char* _pd, uint64_t _v) const override
    {
        return serialization::v3::binary::store(_pd, _v);
    }

    char* storeCrossValue(char* _pd, const size_t _sz, uint32_t _v) const override
    {
        return serialization::v3::binary::cross::store_with_check(_pd, _sz, _v);
    }

    const char* loadValue(const char* _ps, uint8_t& _v) const override
    {
        return serialization::v3::binary::load(_ps, _v);
    }
    const char* loadValue(const char* _ps, uint16_t& _v) const override
    {
        return serialization::v3::binary::load(_ps, _v);
    }
    const char* loadValue(const char* _ps, uint32_t& _v) const override
    {
        return serialization::v3::binary::load(_ps, _v);
    }
    const char* loadValue(const char* _ps, uint64_t& _v) const override
    {
        return serialization::v3::binary::load(_ps, _v);
    }
    const char* loadCrossValue(const char* _ps, const size_t _sz, uint32_t& _v) const override
    {
        return serialization::v3::binary::cross::load_with_check(_ps, _sz, _v);
    }

    size_t typeIndex(const Message* _pmsg) const override
    {
        return type_map_.index(_pmsg);
    }

    void complete(const size_t _idx, ConnectionContext& _rctx, MessagePointerT& _rsent_msg_ptr, MessagePointerT& _rrecv_msg_ptr, ErrorConditionT const& _rerr) const override
    {
        type_data_vec_[_idx].complete(_rctx, _rsent_msg_ptr, _rrecv_msg_ptr, _rerr);
    }

    typename SerializerT::PointerT createSerializer(const WriterConfiguration& /*_rconfig*/) const override
    {
        return std::make_unique<SerializerT>(metadata_factory_, type_map_);
    }

    typename DeserializerT::PointerT createDeserializer(const ReaderConfiguration& /*_rconfig*/) const override
    {
        return std::make_unique<DeserializerT>(metadata_factory_, type_map_);
    }

    void reconfigure(mprpc::Deserializer& /*_rdes*/, const ReaderConfiguration& /*_rconfig*/) const override
    {
    }

    void reconfigure(mprpc::Serializer& /*_rser*/, const WriterConfiguration& /*_rconfig*/) const override
    {
    }

    size_t minimumFreePacketDataSize() const override
    {
        return 16;
    }

protected:
    Protocol() {}
};

template <class MetadataVariant, class TypeId, class MetadataFactory, class InitFunction>
auto create_protocol(MetadataFactory &&_metadata_factory, InitFunction _init_fnc){
    using ProtocolT = Protocol<MetadataVariant, MetadataFactory, TypeId>;
    return std::make_shared<ProtocolT>(_metadata_factory, _init_fnc);
}

} //namespace serialization_v2
} //namespace mprpc
} //namespace frame
} //namespace solid
