// solid/serialization/v2/typemap.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/serialization/v2/typemapbase.hpp"
#include "solid/utility/function.hpp"
#include <unordered_map>
#include <vector>

namespace solid {
namespace serialization {
namespace v2 {

template <typename TypeId, class Ctx, template <typename, class> class Ser, template <typename, class> class Des, class Data>
class TypeMap : protected TypeMapBase {
    typedef void (*CastFunctionT)(void*, void*);

    struct CastStub {
        CastStub(
            CastFunctionT _plain_cast  = nullptr,
            CastFunctionT _shared_cast = nullptr)
            : plain_cast_(_plain_cast)
            , shared_cast_(_shared_cast)
        {
        }

        CastFunctionT plain_cast_;
        CastFunctionT shared_cast_;
    };

    typedef std::pair<std::type_index, size_t> CastIdT;

    struct CastHash {
        size_t operator()(CastIdT const& _id) const
        {
            return _id.first.hash_code() ^ _id.second;
        }
    };

    using LoadFunctionT          = solid_function_t(void(void*, void*, void*, const char*));
    using StoreFunctionT         = solid_function_t(void(void*, const void*, void*, const char*));
    using PlainFactoryFunctionT  = solid_function_t(void*(const CastFunctionT&, const PointerFunctionT&));
    using SharedFactoryFunctionT = solid_function_t(void*(const CastFunctionT&, void*));

    struct TypeStub {
        template <typename D>
        TypeStub(const TypeId& _rid, D&& _ud)
            : id_(_rid)
            , data_(std::forward<D>(_ud))
        {
        }
        TypeStub(const TypeId& _rid)
            : id_(_rid)
        {
        }

        const TypeId           id_;
        PlainFactoryFunctionT  plain_factory_fnc_;
        SharedFactoryFunctionT shared_factory_fnc_;
        StoreFunctionT         store_fnc_;
        LoadFunctionT          load_fnc_;
        Data                   data_;
    };

    using TypeVectorT = std::vector<TypeStub>;
    using TypeIdMapT  = std::unordered_map<TypeId, size_t>;
    typedef std::unordered_map<CastIdT, CastStub, CastHash> CastMapT;
    size_t                                                  null_index_;
    TypeVectorT                                             type_vec_;
    TypeIdMapT                                              type_id_map_;
    CastMapT                                                cast_map_;

public:
    using ContextT      = Ctx;
    using SerializerT   = Ser<TypeId, ContextT>;
    using DeserializerT = Des<TypeId, ContextT>;

    TypeMap()
        : null_index_(InvalidIndex())
    {
    }

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

    size_t null(const TypeId& _rtid)
    {
        solid_check(type_id_map_.find(_rtid) == type_id_map_.end(), "type_id already used");
        type_id_map_[_rtid] = type_vec_.size();
        type_vec_.emplace_back(_rtid);
        
        using PointerFunctionT = TypeMapBase::PointerFunctionT;
        type_vec_.back().plain_factory_fnc_ = [](const CastFunctionT& _cast_fnc, const PointerFunctionT& _ptr_fnc) {
            _ptr_fnc(nullptr);
            return nullptr;
        };

        type_vec_.back().shared_factory_fnc_ = [](const CastFunctionT& _cast_fnc, void* _dest_ptr) {
            return nullptr;
        };

        type_vec_.back().store_fnc_ = [](void* _ps, const void* _p, void* _pc, const char* _name) {
        };
        type_vec_.back().load_fnc_ = [](void* _pd, void* _p, void* _pc, const char* _name) {
        };
        null_index_ = type_vec_.size() - 1;
        return null_index_;
    }

    template <class T>
    size_t index(const T* _p) const
    {
        return TypeMapBase::index(_p);
    }

    const Data& operator[](const size_t _idx) const
    {
        return type_vec_[_idx].data_;
    }

    template <class T>
    size_t registerType(const TypeId& _rtid)
    {
        solid_check(type_id_map_.find(_rtid) == type_id_map_.end(), "type_id already used");
        solid_check(type_map_.find(std::type_index(typeid(T))) == type_map_.end(), "type already registered");
        type_id_map_[_rtid]                   = type_vec_.size();
        type_map_[std::type_index(typeid(T))] = type_vec_.size();

        type_vec_.emplace_back(_rtid);
        
        using PointerFunctionT = TypeMapBase::PointerFunctionT;
        type_vec_.back().plain_factory_fnc_ = [](const CastFunctionT& _cast_fnc, const PointerFunctionT& _ptr_fnc) {
            T*    ptr   = new T;
            void* pdest = nullptr;
            _cast_fnc(ptr, &pdest);
            _ptr_fnc(pdest);
            return ptr;
        };

        type_vec_.back().shared_factory_fnc_ = [](const CastFunctionT& _cast_fnc, void* _dest_ptr) {
            std::shared_ptr<T> sp  = std::make_shared<T>();
            void*              ptr = sp.get();
            _cast_fnc(&sp, _dest_ptr);
            return ptr;
        };

        type_vec_.back().store_fnc_ = [](void* _ps, const void* _p, void* _pc, const char* _name) {
            SerializerT& rser = *reinterpret_cast<SerializerT*>(_ps);
            ContextT&    rctx = *reinterpret_cast<ContextT*>(_pc);
            const T&     rt   = *reinterpret_cast<const T*>(_p);
            rser.add(rt, rctx, _name);
        };
        type_vec_.back().load_fnc_ = [](void* _pd, void* _p, void* _pc, const char* _name) {
            DeserializerT& rdes = *reinterpret_cast<DeserializerT*>(_pd);
            ContextT&      rctx = *reinterpret_cast<ContextT*>(_pc);
            T&             rt   = *reinterpret_cast<T*>(_p);
            rdes.add(rt, rctx, _name);
        };

        registerCast<T, T>();
        registerCast<T>();
        return type_vec_.size() - 1;
    }

    template <class T, class D, class Allocator>
    size_t registerType(D&& _d, Allocator _allocator, const TypeId& _rtid)
    {
        solid_check(type_id_map_.find(_rtid) == type_id_map_.end(), "type_id already used");
        solid_check(type_map_.find(std::type_index(typeid(T))) == type_map_.end(), "type already registered");
        type_id_map_[_rtid]                   = type_vec_.size();
        type_map_[std::type_index(typeid(T))] = type_vec_.size();

        type_vec_.emplace_back(_rtid, _d);

        //TODO: For now we cannot use Allocator with naked pointer as the pointer will be aquired by an unique_ptr
        //so we leave the default allocator
        using PointerFunctionT = TypeMapBase::PointerFunctionT;
        type_vec_.back().plain_factory_fnc_ = [](const CastFunctionT& _cast_fnc, const PointerFunctionT& _ptr_fnc) {
            T*    ptr   = new T;
            void* pdest = nullptr;
            _cast_fnc(ptr, &pdest);
            _ptr_fnc(pdest);
            return ptr;
        };

        type_vec_.back().shared_factory_fnc_ = [_allocator](const CastFunctionT& _cast_fnc, void* _dest_ptr) {
            std::shared_ptr<T> sp  = std::allocate_shared<T>(_allocator);
            void*              ptr = sp.get();
            _cast_fnc(&sp, _dest_ptr);
            return ptr;
        };

        type_vec_.back().store_fnc_ = [](void* _ps, const void* _p, void* _pc, const char* _name) {
            SerializerT& rser = *reinterpret_cast<SerializerT*>(_ps);
            ContextT&    rctx = *reinterpret_cast<ContextT*>(_pc);
            const T&     rt   = *reinterpret_cast<const T*>(_p);
            rser.add(rt, rctx, _name);
        };
        type_vec_.back().load_fnc_ = [](void* _pd, void* _p, void* _pc, const char* _name) {
            DeserializerT& rdes = *reinterpret_cast<DeserializerT*>(_pd);
            ContextT&      rctx = *reinterpret_cast<ContextT*>(_pc);
            T&             rt   = *reinterpret_cast<T*>(_p);
            rdes.add(rt, rctx, _name);
        };

        registerCast<T, T>();
        registerCast<T>();
        return type_vec_.size() - 1;
    }

    template <class T, class D, class StoreF, class LoadF>
    size_t registerType(D&& _d, StoreF _sf, LoadF _lf, const TypeId& _rtid)
    {
        solid_check(type_id_map_.find(_rtid) == type_id_map_.end(), "type_id already used");
        solid_check(type_map_.find(std::type_index(typeid(T))) == type_map_.end(), "type already registered");
        type_id_map_[_rtid]                   = type_vec_.size();
        type_map_[std::type_index(typeid(T))] = type_vec_.size();

        type_vec_.emplace_back(_rtid, std::forward<D>(_d));
        
        using PointerFunctionT = TypeMapBase::PointerFunctionT;
        type_vec_.back().plain_factory_fnc_ = [](const CastFunctionT& _cast_fnc, const PointerFunctionT& _ptr_fnc) {
            T*    ptr   = new T;
            void* pdest = nullptr;
            _cast_fnc(ptr, &pdest);
            _ptr_fnc(pdest);
            return ptr;
        };

        type_vec_.back().shared_factory_fnc_ = [](const CastFunctionT& _cast_fnc, void* _dest_ptr) {
            std::shared_ptr<T> sp  = std::make_shared<T>();
            void*              ptr = sp.get();
            _cast_fnc(&sp, _dest_ptr);
            return ptr;
        };

        type_vec_.back().store_fnc_ = [_sf](void* _ps, const void* _p, void* _pc, const char* _name) {
            SerializerT& rser = *reinterpret_cast<SerializerT*>(_ps);
            ContextT&    rctx = *reinterpret_cast<ContextT*>(_pc);
            const T&     rt   = *reinterpret_cast<const T*>(_p);
            _sf(rser, rt, rctx, _name);
        };
        type_vec_.back().load_fnc_ = [_lf](void* _pd, void* _p, void* _pc, const char* _name) {
            DeserializerT& rdes = *reinterpret_cast<DeserializerT*>(_pd);
            ContextT&      rctx = *reinterpret_cast<ContextT*>(_pc);
            T&             rt   = *reinterpret_cast<T*>(_p);
            _lf(rdes, rt, rctx, _name);
        };

        registerCast<T, T>();
        registerCast<T>();
        return type_vec_.size() - 1;
    }

    template <class T, class D, class StoreF, class LoadF, class Allocator>
    size_t registerType(D&& _d, StoreF _sf, LoadF _lf, Allocator _allocator, const TypeId& _rtid)
    {
        solid_check(type_id_map_.find(_rtid) == type_id_map_.end(), "type_id already used");
        solid_check(type_map_.find(std::type_index(typeid(T))) == type_map_.end(), "type already registered");
        type_id_map_[_rtid]                   = type_vec_.size();
        type_map_[std::type_index(typeid(T))] = type_vec_.size();

        type_vec_.emplace_back(_rtid, _d);

        //TODO: For now we cannot use Allocator with naked pointer as the pointer will be aquired by an unique_ptr
        //so we leave the default allocator
        using PointerFunctionT = TypeMapBase::PointerFunctionT;
        type_vec_.back().plain_factory_fnc_ = [](const CastFunctionT& _cast_fnc, const PointerFunctionT& _ptr_fnc) {
            T*    ptr   = new T;
            void* pdest = nullptr;
            _cast_fnc(ptr, &pdest);
            _ptr_fnc(pdest);
            return ptr;
        };

        type_vec_.back().shared_factory_fnc_ = [_allocator](const CastFunctionT& _cast_fnc, void* _dest_ptr) {
            std::shared_ptr<T> sp  = std::allocate_shared<T>(_allocator);
            void*              ptr = sp.get();
            _cast_fnc(&sp, _dest_ptr);
            return ptr;
        };

        type_vec_.back().store_fnc_ = [_sf](void* _ps, const void* _p, void* _pc, const char* _name) {
            SerializerT& rser = *reinterpret_cast<SerializerT*>(_ps);
            ContextT&    rctx = *reinterpret_cast<ContextT*>(_pc);
            const T&     rt   = *reinterpret_cast<const T*>(_p);
            _sf(rser, rt, rctx, _name);
        };
        type_vec_.back().load_fnc_ = [_lf](void* _pd, void* _p, void* _pc, const char* _name) {
            DeserializerT& rdes = *reinterpret_cast<DeserializerT*>(_pd);
            ContextT&      rctx = *reinterpret_cast<ContextT*>(_pc);
            T&             rt   = *reinterpret_cast<T*>(_p);
            _lf(rdes, rt, rctx, _name);
        };

        registerCast<T, T>();
        registerCast<T>();
        return type_vec_.size() - 1;
    }

    template <class Derived, class Base>
    void registerDownCast()
    {
        const auto it = type_map_.find(std::type_index(typeid(Base)));
        solid_check(it != type_map_.end(), "Base type not registered");

        type_map_[std::type_index(typeid(Derived))] = it->second;
    }

    template <class Derived, class Base>
    void registerCast()
    {
        const auto it = type_map_.find(std::type_index(typeid(Derived)));
        solid_check(it != type_map_.end(), "Derived type not registered");
        solid_check(null_index_ != InvalidIndex(), "Null not set");

        cast_map_[CastIdT(std::type_index(typeid(Base)), it->second)]     = CastStub(cast_plain_pointer<Base, Derived>, cast_shared_pointer<Base, Derived>);
        cast_map_[CastIdT(std::type_index(typeid(Derived)), null_index_)] = &cast_void_pointer<Derived>;
        cast_map_[CastIdT(std::type_index(typeid(Base)), null_index_)]    = &cast_void_pointer<Base>;
    }

    template <class Derived>
    void registerCast()
    {
        const auto it = type_map_.find(std::type_index(typeid(Derived)));
        solid_check(it != type_map_.end(), "Derived type not registered");
        solid_check(null_index_ != InvalidIndex(), "Null not set");

        cast_map_[CastIdT(std::type_index(typeid(Derived)), null_index_)] = &cast_void_pointer<Derived>;
    }

private:
    template <class Base, class Derived>
    static void cast_plain_pointer(void* _pderived, void* _pbase)
    {
        Derived* pd  = reinterpret_cast<Derived*>(_pderived);
        Base*&   rpb = *reinterpret_cast<Base**>(_pbase);
        rpb          = static_cast<Base*>(pd);
    }

    template <class Base, class Derived>
    static void cast_shared_pointer(void* _pderived, void* _pbase)
    {
        using DerivedSharedPtrT = std::shared_ptr<Derived>;
        using BaseSharedPtrT    = std::shared_ptr<Base>;

        DerivedSharedPtrT* pspder = reinterpret_cast<DerivedSharedPtrT*>(_pderived);
        BaseSharedPtrT*    pspbas = reinterpret_cast<BaseSharedPtrT*>(_pbase);

        if (pspder) {
            *pspbas = std::move(std::static_pointer_cast<Base>(std::move(*pspder)));
        } else {
            pspbas->reset();
        }
    }

    template <class T>
    static void cast_void_pointer(void* _pderived, void* _pbase)
    {
        T*& rpb = *reinterpret_cast<T**>(_pbase);
        rpb     = reinterpret_cast<T*>(_pderived);
    }

    void getTypeId(const size_t _idx, void* _ptype_id) const override
    {
        TypeId& rtid = *reinterpret_cast<TypeId*>(_ptype_id);
        rtid         = type_vec_[_idx].id_;
    }

    size_t getNullTypeId(void* _ptype_id) const override
    {
        TypeId& rtid = *reinterpret_cast<TypeId*>(_ptype_id);
        rtid         = type_vec_[null_index_].id_;
        return null_index_;
    }

    ErrorConditionT doSerialize(Base& _rs, const void* _pt, const size_t _idx, const char* _name) const override
    {
        //TODO
        return ErrorConditionT();
    }
    ErrorConditionT doSerialize(Base& _rs, const void* _pt, const size_t _idx, void* _pctx, const char* _name) const override
    {
        const TypeStub& rts = type_vec_[_idx];
        rts.store_fnc_(&_rs, _pt, _pctx, _name);
        return ErrorConditionT();
    }

    ErrorConditionT doDeserializeUnique(Base& _rd, const PointerFunctionT& _fnc, const std::type_index& _spt, const void* _ptype_id, const char* _name) const override
    {
        //TODO:
        return ErrorConditionT();
    }

    ErrorConditionT doDeserializeUnique(Base& _rd, const PointerFunctionT& _fnc, const std::type_index& _spt, const void* _ptype_id, void* _pctx, const char* _name) const override
    {
        const TypeId& rtype_id = *reinterpret_cast<const TypeId*>(_ptype_id);
        const auto    type_it  = type_id_map_.find(rtype_id);
        if (type_it != type_id_map_.end()) {
            const auto cast_it = cast_map_.find(CastIdT(_spt, type_it->second));
            if (cast_it != cast_map_.end()) {
                const TypeStub& rts     = type_vec_[type_it->second];
                void*           realptr = rts.plain_factory_fnc_(cast_it->second.plain_cast_, _fnc);
                rts.load_fnc_(&_rd, realptr, _pctx, _name);
            } else {
                return error_no_cast();
            }
        } else {
            return error_no_type();
        }
        return ErrorConditionT();
    }

    ErrorConditionT doDeserializeShared(Base& _rd, void* _psp, const std::type_index& _spt, const void* _ptype_id, const char* _name) const override
    {
        //TODO:
        return ErrorConditionT();
    }
    ErrorConditionT doDeserializeShared(Base& _rd, void* _psp, const std::type_index& _spt, const void* _ptype_id, void* _pctx, const char* _name) const override
    {
        const TypeId& rtype_id = *reinterpret_cast<const TypeId*>(_ptype_id);
        const auto    type_it  = type_id_map_.find(rtype_id);
        if (type_it != type_id_map_.end()) {
            const auto cast_it = cast_map_.find(CastIdT(_spt, type_it->second));
            if (cast_it != cast_map_.end()) {
                const TypeStub& rts     = type_vec_[type_it->second];
                void*           realptr = rts.shared_factory_fnc_(cast_it->second.shared_cast_, _psp);
                rts.load_fnc_(&_rd, realptr, _pctx, _name);
            } else {
                return error_no_cast();
            }
        } else {
            return error_no_type();
        }
        return ErrorConditionT();
    }
};

} //namespace v2
} //namespace serialization
} // namespace solid
