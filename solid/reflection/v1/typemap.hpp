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
    using ReverseCastFunctionT = std::function<const void*(const void*)>;
    
    struct CastStub{
        template <class SF, class UF, class RF>
        CastStub(SF _sf, UF _uf, RF _rf):shared_fnc_(_sf), unique_fnc_(_uf), reverse_fnc_(_rf){}
        CastFunctionT shared_fnc_;
        CastFunctionT unique_fnc_;
        ReverseCastFunctionT reverse_fnc_;
    };
    using TypeIndexPairT   = std::pair<std::type_index, std::type_index>;
    struct TypeIndexPairHasher{
        size_t operator()(const TypeIndexPairT &_pair)const{
            return _pair.first.hash_code() ^ _pair.second.hash_code();
        }
    };
    using CastMapT         = std::unordered_map<TypeIndexPairT, size_t, TypeIndexPairHasher>;
    using CastVectorT      = std::vector<CastStub>;
    using ReflectFunctionT = std::function<void(void*, const void*, void*, const ReverseCastFunctionT&)>;
    using CreateFunctionT = std::function<void(void*, void*, const CastFunctionT&)>;
    struct ReflectorStub{
        ReflectFunctionT    reflect_fnc_;
        CreateFunctionT     create_shared_fnc_;
        CreateFunctionT     create_unique_fnc_;
    };
    using ReflectorVectorT = std::vector<ReflectorStub>;
    struct TypeStub{
        ReflectorVectorT    reflector_vec_;
        std::type_index     base_type_index_ = std::type_index{typeid(void)};
        size_t              base_cast_index_ = 0;
        std::type_index     this_type_index_ = std::type_index{typeid(void)};
        size_t              this_cast_index_ = 0;
    };
    
    using TypeNameMapT = std::unordered_map<std::string_view, size_t>;
    using TypeIdMapT = std::unordered_map<size_t, size_t>;
    
    struct CategoryStub{
        TypeNameMapT type_name_map_;
        TypeIdMapT   type_id_map_;
    };
    
    using IndexTupleT      = std::tuple<size_t, size_t, size_t>;//index, category, id
    using TypeIndexMapT    = std::unordered_map<std::type_index, IndexTupleT>;
    using TypeVectorT      = std::vector<TypeStub>;
    using CategoryVectorT  = std::vector<CategoryStub>;
    
    TypeIndexMapT       type_index_map_;
    TypeVectorT         type_vec_;
    CastMapT            cast_map_;
    CastVectorT         cast_vec_;
    CategoryVectorT     category_vec_;
    std::vector<size_t>& reflector_index_vec_;
    
    
    template <class Reflector>
    size_t reflectorIndex()const{
        solid_assert(reflector_index_v<Reflector> < reflector_index_vec_.size());
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
    using IdTupleT = std::tuple<size_t, size_t, size_t>;
    virtual ~TypeMapBase(){}

    template <typename Reflector, class T>
    void reflect(Reflector &_rreflector, T &_rvalue, typename Reflector::ContextT &_rctx)const{
        const auto reflector_index = reflectorIndex<Reflector>();
        solid_assert(reflector_index != solid::InvalidIndex());
        
        const auto it = type_index_map_.find(std::type_index(typeid(_rvalue)));
        
        if(it != type_index_map_.end()){
            const auto &rtypestub = type_vec_[std::get<0>(it->second)];
            size_t cast_index = InvalidIndex{};
            const auto elem_type_index = std::type_index(typeid(T));
            if(rtypestub.base_type_index_ == elem_type_index){
                cast_index = rtypestub.base_cast_index_;
            }else if(rtypestub.this_type_index_ == elem_type_index){
                cast_index = rtypestub.this_cast_index_;
            }else{
                //need to search for the index
                const auto it = cast_map_.find(std::make_pair(rtypestub.this_type_index_, elem_type_index));
                solid_check(it != cast_map_.end());
                cast_index = it->second;
            }
            const auto& rcaststub = cast_vec_[cast_index];
            
            rtypestub.reflector_vec_[reflector_index].reflect_fnc_(&_rreflector, &_rvalue, &_rctx, rcaststub.reverse_fnc_);
        }else{
            solid_throw("Unknown type: "<<typeid(_rvalue).name());
        }
    }
    
    template <typename Reflector, class T>
    void reflect(Reflector &_rreflector, T &_rvalue, typename Reflector::ContextT &_rctx, const IdTupleT &_id)const{
        const size_t reflector_index = reflectorIndex<Reflector>();
        solid_assert(reflector_index != solid::InvalidIndex());
        
        const auto &rtypestub = type_vec_[std::get<0>(_id)];
        size_t cast_index = InvalidIndex{};
        const auto elem_type_index = std::type_index(typeid(T));
        if(rtypestub.base_type_index_ == elem_type_index){
            cast_index = rtypestub.base_cast_index_;
        }else if(rtypestub.this_type_index_ == elem_type_index){
            cast_index = rtypestub.this_cast_index_;
        }else{
            //need to search for the index
            const auto it = cast_map_.find(std::make_pair(rtypestub.this_type_index_, elem_type_index));
            solid_check(it != cast_map_.end());
            cast_index = it->second;
        }
        const auto& rcaststub = cast_vec_[cast_index];
        
        rtypestub.reflector_vec_[reflector_index].reflect_fnc_(&_rreflector, &_rvalue, &_rctx, rcaststub.reverse_fnc_);
    }
    
    template <class Reflector, class Ptr>
    size_t create(typename Reflector::ContextT &_rctx, Ptr& _rptr, const size_t _category, const std::string_view _name)const{
        static_assert(solid::is_shared_ptr_v<Ptr> || solid::is_unique_ptr_v<Ptr>);
        const size_t reflector_index = reflectorIndex<Reflector>();
        solid_assert(reflector_index != solid::InvalidIndex());
        
        solid_assert(_category < category_vec_.size());
        const auto it = category_vec_[_category].type_name_map_.find(_name);
        if(it != category_vec_[_category].type_name_map_.end()){
            const TypeStub& rtypestub = type_vec_[it->second];
            const auto elem_type_index = std::type_index(typeid(typename Ptr::element_type));
            size_t cast_index = InvalidIndex{};
            if(rtypestub.base_type_index_ == elem_type_index){
                cast_index = rtypestub.base_cast_index_;
            }else if(rtypestub.this_type_index_ == elem_type_index){
                cast_index = rtypestub.this_cast_index_;
            }else{
                //need to search for the index
                const auto it = cast_map_.find(std::make_pair(rtypestub.this_type_index_, elem_type_index));
                solid_check(it != cast_map_.end());
                cast_index = it->second;
            }
            
            const auto& rcaststub = cast_vec_[cast_index];
            
            if constexpr (solid::is_shared_ptr_v<Ptr>){
                rtypestub.reflector_vec_[reflector_index].create_shared_fnc_(&_rctx, &_rptr, rcaststub.shared_fnc_);
            }else{//unique_ptr
                static_assert(std::is_same_v<Ptr, std::unique_ptr<typename Ptr::element_type>>, "Only unique with default deleter is supported");
                rtypestub.reflector_vec_[reflector_index].create_unique_fnc_(&_rctx, &_rptr, rcaststub.unique_fnc_);
            }
            return it->second;
        }else{
            return 0;
        }
    }
    
    template <class Reflector, class Ptr>
    size_t create(typename Reflector::ContextT &_rctx, Ptr& _rptr, const size_t _category, const size_t _id)const{
        static_assert(solid::is_shared_ptr_v<Ptr> || solid::is_unique_ptr_v<Ptr>);
        const size_t reflector_index = reflectorIndex<Reflector>();
        solid_assert(reflector_index != solid::InvalidIndex());
        
        solid_assert(_category < category_vec_.size());
        _rptr.reset(nullptr);
        const auto it = category_vec_[_category].type_id_map_.find(_id);
        if(it != category_vec_[_category].type_id_map_.end()){
            const TypeStub& rtypestub = type_vec_[it->second];
            const auto elem_type_index = std::type_index(typeid(typename Ptr::element_type));
            size_t cast_index = InvalidIndex{};
            if(rtypestub.base_type_index_ == elem_type_index){
                cast_index = rtypestub.base_cast_index_;
            }else if(rtypestub.this_type_index_ == elem_type_index){
                cast_index = rtypestub.this_cast_index_;
            }else{
                //need to search for the index
                const auto it = cast_map_.find(std::make_pair(rtypestub.this_type_index_, elem_type_index));
                solid_check(it != cast_map_.end());
                cast_index = it->second;
            }
            
            const auto& rcaststub = cast_vec_[cast_index];
            
            if constexpr (solid::is_shared_ptr_v<Ptr>){
                rtypestub.reflector_vec_[reflector_index].create_shared_fnc_(&_rctx, &_rptr, rcaststub.shared_fnc_);
            }else{//unique_ptr
                static_assert(std::is_same_v<Ptr, std::unique_ptr<typename Ptr::element_type>>, "Only unique with default deleter is supported");
                rtypestub.reflector_vec_[reflector_index].create_unique_fnc_(&_rctx, &_rptr, rcaststub.unique_fnc_);
            }
            return it->second;
        }else{
            return 0;
        }
    }
    
    template <class Reflector, class Ptr>
    size_t createAndReflect(Reflector &_rreflector, Ptr& _rptr, typename Reflector::ContextT &_rctx, const size_t _category, const std::string_view _name)const{
        const size_t reflector_index = reflectorIndex<Reflector>();
        solid_assert(reflector_index != solid::InvalidIndex());
        static_assert(solid::is_shared_ptr_v<Ptr> || solid::is_unique_ptr_v<Ptr>);
        
        solid_assert(_category < category_vec_.size());
        _rptr.reset(nullptr);
        const auto it = category_vec_[_category].type_name_map_.find(_name);
        if(it != category_vec_[_category].type_name_map_.end()){
            const TypeStub& rtypestub = type_vec_[it->second];
            const auto elem_type_index = std::type_index(typeid(typename Ptr::element_type));
            size_t cast_index = InvalidIndex{};
            if(rtypestub.base_type_index_ == elem_type_index){
                cast_index = rtypestub.base_cast_index_;
            }else if(rtypestub.this_type_index_ == elem_type_index){
                cast_index = rtypestub.this_cast_index_;
            }else{
                //need to search for the index
                const auto it = cast_map_.find(std::make_pair(rtypestub.this_type_index_, elem_type_index));
                solid_check(it != cast_map_.end());
                cast_index = it->second;
            }
            
            const auto& rcaststub = cast_vec_[cast_index];
            
            if constexpr (solid::is_shared_ptr_v<Ptr>){
                rtypestub.reflector_vec_[reflector_index].create_shared_fnc_(&_rctx, &_rptr, rcaststub.shared_fnc_);
            }else{//unique_ptr
                static_assert(std::is_same_v<Ptr, std::unique_ptr<typename Ptr::element_type>>, "Only unique with default deleter is supported");
                rtypestub.reflector_vec_[reflector_index].create_unique_fnc_(&_rctx, &_rptr, rcaststub.unique_fnc_);
            }
            
            rtypestub.reflector_vec_[reflector_index].reflect_fnc_(&_rreflector, _rptr.get(), &_rctx, rcaststub.reverse_fnc_);
            return it->second;
        }else{
            return 0;
        }
    }
    
    template <class Reflector, class Ptr>
    size_t createAndReflect(Reflector &_rreflector, Ptr& _rptr, typename Reflector::ContextT &_rctx, const size_t _category, const size_t _id)const{
        const size_t reflector_index = reflectorIndex<Reflector>();
        solid_assert(reflector_index != solid::InvalidIndex());
        static_assert(solid::is_shared_ptr_v<Ptr> || solid::is_unique_ptr_v<Ptr>);
        
        solid_assert(_category < category_vec_.size());
        _rptr.reset();
        const auto it = category_vec_[_category].type_id_map_.find(_id);
        if(it != category_vec_[_category].type_id_map_.end()){
            const TypeStub& rtypestub = type_vec_[it->second];
            const auto elem_type_index = std::type_index(typeid(typename Ptr::element_type));
            size_t cast_index = InvalidIndex{};
            if(rtypestub.base_type_index_ == elem_type_index){
                cast_index = rtypestub.base_cast_index_;
            }else if(rtypestub.this_type_index_ == elem_type_index){
                cast_index = rtypestub.this_cast_index_;
            }else{
                //need to search for the index
                const auto it = cast_map_.find(std::make_pair(rtypestub.this_type_index_, elem_type_index));
                solid_check(it != cast_map_.end());
                cast_index = it->second;
            }
            
            const auto& rcaststub = cast_vec_[cast_index];
            
            if constexpr (solid::is_shared_ptr_v<Ptr>){
                rtypestub.reflector_vec_[reflector_index].create_shared_fnc_(&_rctx, &_rptr, rcaststub.shared_fnc_);
            }else{//unique_ptr
                static_assert(std::is_same_v<Ptr, std::unique_ptr<typename Ptr::element_type>>, "Only unique with default deleter is supported");
                rtypestub.reflector_vec_[reflector_index].create_unique_fnc_(&_rctx, &_rptr, rcaststub.unique_fnc_);
            }
            
            rtypestub.reflector_vec_[reflector_index].reflect_fnc_(&_rreflector, _rptr.get(), &_rctx, rcaststub.reverse_fnc_);
            return it->second;
        }else{
            return 0;
        }
    }

    template <class T>
    size_t index(const T* _pvalue) const
    {
        if(_pvalue != nullptr){
            const auto it = type_index_map_.find(std::type_index(typeid(*_pvalue)));
            solid_check(it != type_index_map_.end(), "Unknown type: "<<typeid(*_pvalue).name());
            return std::get<0>(it->second);
        } else {
            return 0;
        }
    }

    template <class T>
    auto id(const T *_pvalue)const{
        if(_pvalue != nullptr){
            const auto index = std::type_index(typeid(*_pvalue));
            const auto it = type_index_map_.find(index);
            //solid_check(it != type_index_map_.end(), "Unknown type: "<<typeid(*_pvalue).name());
            solid_assert(it != type_index_map_.end());
            if(it != type_index_map_.end()){
                return it->second;
                //return std::pair<size_t, size_t>(std::get<1>(it->second), std::get<2>(it->second));
            }
        }
        return std::make_tuple(static_cast<size_t>(0), static_cast<size_t>(0), static_cast<size_t>(0));
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
            auto init_lambda = [](auto &_rctx, auto &_rptr){
                using PtrType = std::decay_t<decltype(_rptr)>;
                if constexpr (solid::is_shared_ptr_v<PtrType>){
                    _rptr = std::make_shared<typename PtrType::element_type>();
                }else if constexpr (solid::is_unique_ptr_v<PtrType>){
                    _rptr = std::make_unique<typename PtrType::element_type>();
                }
            };
            const size_t rv = rtype_map_.doRegisterType<T>(_category, _id, _name, init_lambda, rtype_map_.doRegisterCast<T, T>());
            return rv;
        }
        template <class T, class B>
        size_t registerType(const size_t _category, const size_t _id, const std::string_view _name){
            auto init_lambda = [](auto &_rctx, auto &_rptr){
                using PtrType = std::decay_t<decltype(_rptr)>;
                if constexpr (solid::is_shared_ptr_v<PtrType>){
                    _rptr = std::make_shared<typename PtrType::element_type>();
                }else if constexpr (solid::is_unique_ptr_v<PtrType>){
                    _rptr = std::make_unique<typename PtrType::element_type>();
                }
            };
            const size_t rv = rtype_map_.doRegisterType<T>(_category, _id, _name, init_lambda, rtype_map_.doRegisterCast<T, T>(), std::type_index{typeid(B)}, rtype_map_.doRegisterCast<T, B>());
            return rv;
        }
        
        template <class T, class InitF>
        size_t registerType(const size_t _category, const size_t _id, const std::string_view _name, InitF _init_f){
            const size_t rv = rtype_map_.doRegisterType<T>(_category, _id, _name, _init_f, rtype_map_.doRegisterCast<T, T>());
            return rv;
        }
        template <class T, class B, class InitF>
        size_t registerType(const size_t _category, const size_t _id, const std::string_view _name, InitF _init_f){
            const size_t rv = rtype_map_.doRegisterType<T>(_category, _id, _name, _init_f, rtype_map_.doRegisterCast<T, T>(), std::type_index{typeid(B)}, rtype_map_.doRegisterCast<T, B>());
            return rv;
        }
        
        template <class T, class B>
        void registerCast(){
            static_assert(std::is_base_of_v<B, T>, "B not a base of T");
            rtype_map_.doRegisterCast<T, B>();
        }
    };
private:
    
    template <class InitF, class T, class Ref, class ...Rem>
    void doInitTypeReflect(InitF _init_f, const size_t _type_index, const size_t _index = 0){
        type_vec_[_type_index].reflector_vec_[_index].reflect_fnc_ = [](void *_pref, const void *_pval, void *_pctx, const ReverseCastFunctionT &_reverse_cast_fnc){
            Ref &rref = *reinterpret_cast<Ref*>(_pref);
            typename Ref::ContextT &rctx = *reinterpret_cast<typename Ref::ContextT*>(_pctx);
            
            if constexpr (Ref::is_const_reflector){
                const T& rval = *reinterpret_cast<const T*>(_reverse_cast_fnc(_pval));
                rref.add(rval, rctx, 0, "");
            }else{
                T& rval = *reinterpret_cast<T*>(const_cast<void*>(_reverse_cast_fnc(_pval)));
                rref.add(rval, rctx, 0, "");
            }
        };
        type_vec_[_type_index].reflector_vec_[_index].create_shared_fnc_ = [_init_f](void *_pctx, void*_pptr, const CastFunctionT& _cast){
            //auto ptr = std::make_shared<T>();
            typename Ref::ContextT &rctx = *reinterpret_cast<typename Ref::ContextT*>(_pctx);
            std::shared_ptr<T>   ptr;
            _init_f(rctx, ptr);
            _cast(_pptr, &ptr);
        };
        type_vec_[_type_index].reflector_vec_[_index].create_unique_fnc_ = [_init_f](void *_pctx, void*_pptr, const CastFunctionT& _cast){
            //auto ptr = std::make_unique<T>();
            typename Ref::ContextT &rctx = *reinterpret_cast<typename Ref::ContextT*>(_pctx);
            std::unique_ptr<T>   ptr;
            _init_f(rctx, ptr);
            _cast(_pptr, &ptr);
        };
        if constexpr (!is_empty_pack<Rem...>::value){
            doInitTypeReflect<InitF, T, Rem...>(_init_f, _type_index, _index + 1);
        }
    }
    
    template <class T, class InitF>
    size_t doRegisterType(
        const size_t _category, const size_t _id, const std::string_view _name,
        InitF _init_f,
        const size_t _this_cast_index,
        const std::type_index _base_type_index = std::type_index{typeid(void)},
        const size_t _base_cast_index = 0
    ){
        solid_check(type_index_map_.find(std::type_index(typeid(T))) == type_index_map_.end(), "Type "<<typeid(T).name()<<" already registered");
        const size_t index = type_vec_.size();
        type_index_map_[std::type_index(typeid(T))] = std::make_tuple(index, _category, _id);
        if(category_vec_.size() <= _category){
            category_vec_.resize(_category + 1);
        }
        CategoryStub &rcategory_stub = category_vec_[_category];
        solid_check(rcategory_stub.type_name_map_.find(_name) == rcategory_stub.type_name_map_.end(), "Name "<<_name<<" already exist in category "<<_category);
        solid_check(rcategory_stub.type_id_map_.find(_id) == rcategory_stub.type_id_map_.end(), "Id "<<_id<<" already exist in category "<<_category);
        rcategory_stub.type_name_map_[_name] = index;
        rcategory_stub.type_id_map_[_id] = index;
        
        type_vec_.emplace_back();
        type_vec_.back().this_cast_index_ = _this_cast_index;
        type_vec_.back().this_type_index_ = std::type_index(typeid(T));
        type_vec_.back().base_type_index_ = _base_type_index;
        type_vec_.back().base_cast_index_ = _base_cast_index;
        type_vec_.back().reflector_vec_.resize(reflector_index_vec.size());
        
        doInitTypeReflect<InitF, T, Reflector...>(_init_f, index);
        return index;
    }
    
    template <class T, class B>
    size_t doRegisterCast(){
        
        const size_t cast_index = cast_vec_.size();
        cast_map_[std::make_pair(std::type_index(typeid(T)), std::type_index(typeid(B)))] = cast_index;
        
        cast_vec_.emplace_back(
            [](void *_pto, void *_pfrom){
                std::shared_ptr<B> &rto = *reinterpret_cast<std::shared_ptr<B>*>(_pto);
                std::shared_ptr<T> &rfrom = *reinterpret_cast<std::shared_ptr<T>*>(_pfrom);
                rto = std::move(rfrom);
            },
            [](void *_pto, void *_pfrom){
                std::unique_ptr<B> &rto = *reinterpret_cast<std::unique_ptr<B>*>(_pto);
                std::unique_ptr<T> &rfrom = *reinterpret_cast<std::unique_ptr<T>*>(_pfrom);
                rto = std::move(rfrom);
            },
            [](const void *_pfrom){
                return static_cast<const T*>(reinterpret_cast<const B*>(_pfrom));
            }
        );
        return cast_index;
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
