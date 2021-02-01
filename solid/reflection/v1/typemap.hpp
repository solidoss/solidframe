// solid/reflection/v1/typemap.hpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#pragma once

#include <typeinfo>
#include <typeindex>
#include <variant>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>

#include "solid/reflection/v1/typetraits.hpp"
#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/common.hpp"

namespace solid{
namespace reflection{
namespace v1{

inline size_t reflector_index(){
    static std::atomic<size_t> index{0};
    return index.fetch_add(1);
}

template <class Reflector>
inline const size_t reflector_index_v = {reflector_index()};

class TypeMapBase: NonCopyable{
    //using VariantT = std::variant<Reflector...>;
    //using TupleT = std::tuple<Reflector...>;
protected:    
    using CastFunctionT = std::function<void(void*, void*)>;
    
    struct CastStub{
        CastFunctionT shared_fnc_;
        CastFunctionT unique_fnc_;
    };
    
    using CastMapT         = std::unordered_map<std::type_index, CastStub>;
    using ReflectFunctionT = std::function<void(void*, const void*, void*)>;
    struct ReflectorStub{
        ReflectFunctionT    reflect_fnc_;
    };
    using ReflectorVectorT = std::vector<ReflectorStub>;
    using CreateFunctionT = std::function<void(void*, const CastFunctionT&)>;
    struct TypeStub{
        ReflectorVectorT    reflector_vec_;
        CreateFunctionT     create_shared_fnc_;
        CreateFunctionT     create_unique_fnc_;
        CastMapT            cast_map_;
    };
    
    using TypeNameMapT = std::unordered_map<std::string_view, size_t>;
    
    struct CategoryStub{
        TypeNameMapT type_name_map_;
    };
    
    using TypeIndexMapT    = std::unordered_map<std::type_index, size_t>;
    using TypeVectorT      = std::vector<TypeStub>;
    using CategoryVectorT  = std::vector<CategoryStub>;
    
    TypeIndexMapT       type_index_map_;
    TypeVectorT         type_vec_;
    CategoryVectorT     category_vec_;
    std::vector<size_t>& reflector_index_vec_;
    
    
    template <class Reflector>
    size_t reflectorIndex()const{
        return reflector_index_vec_[reflector_index_v<Reflector>];
    }
protected:
    template <class Reflector>
    void reflectorIndex(const size_t _index){
        if(reflector_index_v<Reflector> >= reflector_index_vec_.size()){
            reflector_index_vec_.resize(reflector_index_v<Reflector> + 1);
        }
        reflector_index_vec_[reflector_index_v<Reflector>] = _index;
    }
    typedef void(*IndexInitFncT)(const size_t);
    
    TypeMapBase(std::vector<size_t>& _reflector_index_vec): reflector_index_vec_(_reflector_index_vec){
        type_vec_.emplace_back();//reserve 0 index for for null
    }
public:
    virtual ~TypeMapBase(){}

    template <class Reflector, class T>
    void reflect(Reflector &_rreflector, T &_rvalue, typename Reflector::context_type &_rctx)const{
        const size_t reflector_index = reflectorIndex<Reflector>();
        solid_assert(reflector_index != solid::InvalidIndex());
        
        const auto it = type_index_map_.find(std::type_index(typeid(_rvalue)));
        
        if(it != type_index_map_.end()){
            type_vec_[it->second].reflector_vec_[reflector_index].reflect_fnc_(&_rreflector, &_rvalue, &_rctx);
        }else{
            solid_throw("Unknown type: "<<typeid(_rvalue).name());
        }
    }
    
    template <class Ptr>
    size_t create(Ptr& _rptr, const size_t _category, const std::string_view _name)const{
        static_assert(solid::is_shared_ptr_v<Ptr> || solid::is_unique_ptr_v<Ptr>);
        
        solid_assert(_category < category_vec_.size());
        const auto it = category_vec_[_category].type_name_map_.find(_name);
        if(it != category_vec_[_category].type_name_map_.end()){
            const TypeStub& rtypestub = type_vec_[it->second];
            const auto cast_it = rtypestub.cast_map_.find(std::type_index(typeid(typename Ptr::element_type)));
            if(cast_it != rtypestub.cast_map_.end()){
                
                if constexpr (solid::is_shared_ptr_v<Ptr>){
                    rtypestub.create_shared_fnc_(&_rptr, cast_it->second.shared_fnc_);
                }else{//unique_ptr
                    static_assert(std::is_same_v<Ptr, std::unique_ptr<typename Ptr::element_type>>, "Only unique with default deleter is supported");
                    rtypestub.create_unique_fnc_(&_rptr, cast_it->second.unique_fnc_);
                }
            }
            return it->second;
        }else{
            return 0;
        }
    }
    
    template <class Reflector, class Ptr>
    void createAndReflect(Reflector &_rreflector, Ptr& _rptr, const std::string_view _name)const{
        
    }
    
    template <class Reflector, class Ptr>
    void createAndReflect(Reflector &_rreflector, Ptr& _rptr, const size_t _index)const{
        
    }
    
    template <class T>
    size_t index(const T* _pvalue) const
    {
        solid_check(_pvalue != nullptr);
        const auto it = type_index_map_.find(std::type_index(typeid(*_pvalue)));
        solid_check(it != type_index_map_.end(), "Unknown type: "<<typeid(*_pvalue).name());
        return it->second;
    }
};

template <class ...Reflector>
class TypeMap: public TypeMapBase{
    //using VariantT = std::variant<Reflector...>;
    //using TupleT = std::tuple<Reflector...>;
    static std::vector<size_t> reflector_index_vec;
    
    template <class T, class ...Rem>
    void initReflectorIndex(const size_t _index = 0){
        reflectorIndex<T>(_index);
        if constexpr (!is_empty_pack<Rem...>::value){
            initReflectorIndex<Rem...>(_index + 1);
        }
    }
    
    using ThisT = TypeMap<Reflector...>;
    
    struct Proxy{
        ThisT &rtype_map_;
        
        Proxy(ThisT &_rtype_map):rtype_map_(_rtype_map){}
        
        template <class T>
        size_t registerType(const size_t _category, const size_t _id, const std::string_view _name){
            return rtype_map_.doRegisterType<T>(_category, _id, _name);
        }
        template <class T, class B>
        size_t registerType(const size_t _category, const size_t _id, const std::string_view _name){
            const size_t rv = rtype_map_.doRegisterType<T>(_category, _id, _name);
            rtype_map_.doRegisterCast<T, B>();
            rtype_map_.doRegisterCast<T, T>();
            return rv;
        }
        template <class T, class B>
        void registerCast(){
            static_assert(std::is_base_of_v<B, T>, "B not a base of T");
            rtype_map_.doRegisterCast<T, B>();
        }
    };
private:
    
    template <class T, class Ref, class ...Rem>
    void doInitTypeReflect(const size_t _type_index, const size_t _index = 0){
        type_vec_[_type_index].reflector_vec_[_index].reflect_fnc_ = [](void *_pref, const void *_pval, void *_pctx){
            Ref &rref = *reinterpret_cast<Ref*>(_pref);
            typename Ref::context_type &rctx = *reinterpret_cast<typename Ref::context_type*>(_pctx);
            
            if constexpr (Ref::is_const_reflector::value){
                const T& rval = *reinterpret_cast<const T*>(_pval);
                rref.add(rval, rctx, 0, "");
            }else{
                T& rval = *reinterpret_cast<T*>(const_cast<void*>(_pval));
                rref.add(rval, rctx, 0, "");
            }
        };
        if constexpr (!is_empty_pack<Rem...>::value){
            doInitTypeReflect<T, Rem...>(_type_index, _index + 1);
        }
    }
    
    template <class T>
    size_t doRegisterType(const size_t _category, const size_t _id, const std::string_view _name){
        solid_check(type_index_map_.find(std::type_index(typeid(T))) == type_index_map_.end(), "Type "<<typeid(T).name()<<" already registered");
        const size_t index = type_vec_.size();
        type_index_map_[std::type_index(typeid(T))] = index;;
        if(category_vec_.size() <= _category){
            category_vec_.resize(_category + 1);
        }
        CategoryStub &rcategory_stub = category_vec_[_category];
        solid_check(rcategory_stub.type_name_map_.find(_name) == rcategory_stub.type_name_map_.end(), "Name "<<_name<<" already exist in category "<<_category);
        rcategory_stub.type_name_map_[_name] = index;
        
        type_vec_.emplace_back();
        
        type_vec_.back().create_shared_fnc_ = [](void*_pptr, const CastFunctionT& _cast){
            auto ptr = std::make_shared<T>();
            _cast(_pptr, &ptr);
        };
        type_vec_.back().create_unique_fnc_ = [](void*_pptr, const CastFunctionT& _cast){
            auto ptr = std::make_unique<T>();
            _cast(_pptr, &ptr);
        };
        type_vec_.back().reflector_vec_.resize(reflector_index_vec.size());
        doInitTypeReflect<T, Reflector...>(index);
        return index;
    }
    
    template <class T, class B>
    void doRegisterCast(){
        auto it = type_index_map_.find(std::type_index(typeid(T)));
        solid_check(it != type_index_map_.end(), "Type "<<typeid(T).name()<<" not registered");
        TypeStub& rtypestub = type_vec_[it->second];
        rtypestub.cast_map_[std::type_index(typeid(B))] = {
            [](void *_pto, void *_pfrom){
                std::shared_ptr<B> &rto = *reinterpret_cast<std::shared_ptr<B>*>(_pto);
                std::shared_ptr<T> &rfrom = *reinterpret_cast<std::shared_ptr<T>*>(_pfrom);
                rto = std::move(rfrom);
            },
            [](void *_pto, void *_pfrom){
                std::unique_ptr<B> &rto = *reinterpret_cast<std::unique_ptr<B>*>(_pto);
                std::unique_ptr<T> &rfrom = *reinterpret_cast<std::unique_ptr<T>*>(_pfrom);
                rto = std::move(rfrom);
            }
        };
    }
public:
    template <class InitFnc>
    TypeMap(InitFnc _init_fnc):TypeMapBase(reflector_index_vec){
        static struct Init{
            Init(ThisT &_rthis){
                _rthis.initReflectorIndex<Reflector...>();
            }
        }init{*this};
        Proxy proxy(*this); 
        _init_fnc(proxy);
    }
};

template <class ...Reflector>
inline std::vector<size_t> TypeMap<Reflector...>::reflector_index_vec;

}//namespace v1
}//namespace reflection
}//namespace solid
