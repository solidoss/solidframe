#pragma once

#include "solid/serialization/v2/typemapbase.hpp"

namespace solid {
namespace serialization {
namespace v2 {

template <typename TypeId, class Ctx, template <typename, class> class Ser, template <typename, class> class Des, class Data>
class TypeMap : protected TypeMapBase {
public:
    using ContextT      = Ctx;
    using SerializerT   = Ser<TypeId, ContextT>;
    using DeserializerT = Des<TypeId, ContextT>;

    template <typename... Args>
    SerializerT createSerializer(Args&&... _args) const
    {
        return TypeMapBase::createSerializer<SerializerT>(std::forward<Args>(_args)...);
    }

    template <typename... Args>
    DeserializerT createDeserializer(Args&&... _args) const
    {
        return TypeMapBase::createDeserializer<DeserializerT>(std::forward<Args>(_args)...);
    }

    void null(const TypeId& _rtid)
    {
        null_id_ = _rtid;
    }

    template <class T>
    size_t index(const T* _p) const
    {
        return InvalidIndex();
    }

    const Data& operator[](const size_t _idx) const
    {
        static const Data d;
        return d;
    }

    template <class T>
    size_t registerType(const TypeId& _rtid)
    {
        return InvalidIndex();
    }

    template <class T, class D, class Allocator>
    size_t registerType(D&& _d, Allocator&& _allocator, const TypeId& _rtid)
    {
        return InvalidIndex();
    }

    template <class T, class D, class StoreF, class LoadF>
    size_t registerType(D&& _d, StoreF _sf, LoadF _lf, const TypeId& _rtid)
    {
        return InvalidIndex();
    }

    template <class T, class D, class StoreF, class LoadF, class Allocator>
    size_t registerType(D&& _d, StoreF _sf, LoadF _lf, Allocator _allocator, const TypeId& _rtid)
    {
        return InvalidIndex();
    }

    template <class Derived, class Base>
    bool registerDownCast()
    {
        return false;
    }

    template <class Derived, class Base>
    bool registerCast()
    {
        return false;
    }

private:
    TypeId null_id_;
};

} //namespace v2
} //namespace serialization
} // namespace solid
