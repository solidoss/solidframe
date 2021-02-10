// solid/reflection/v1/reflector.hpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <initializer_list>
#include <typeindex>
#include <functional>
#include <utility>
#include <iostream>
#include "solid/system/common.hpp"
#include "solid/utility/delegate.hpp"
#include "dispatch.hpp"
#include "typemap.hpp"
#include "enummap.hpp"

namespace solid{
namespace reflection{
namespace v1{

template <class Reflector>
class BaseNode{
protected:
    using ContextT = typename Reflector::ContextT;
    
    const Reflector &rreflector_;
    const TypeGroupE   type_group_;
    const std::type_info * const ptype_info_;

    BaseNode(
        Reflector &_rreflector,
        const TypeGroupE _type_group,
        const std::type_info * const _ptype_info
    ): rreflector_(_rreflector), type_group_(_type_group), ptype_info_(_ptype_info){}
    
public:
    virtual ~BaseNode(){}

    TypeGroupE typeGroup()const {
        return type_group_;
    }
    
    const std::type_info& typeInfo()const{
        return *ptype_info_;
    }
    template <TypeGroupE TG>
    auto *as();
    
    template <TypeGroupE TG>
    auto *as()const;
protected:
    virtual void doForEach(Reflector &/*_rreflector*/, ContextT &/*_rctx*/) = 0;
    virtual void doForEach(Reflector &/*_rreflector*/, ContextT &/*_rctx*/) const = 0;
    virtual bool isNull()const = 0;
    virtual void doVisit(Reflector &/*_rreflector*/, const TypeMapBase*, ContextT &/*_rctx*/) = 0;
    virtual void doVisit(Reflector &/*_rreflector*/, const TypeMapBase*, ContextT &/*_rctx*/) const = 0;
    virtual std::ostream& print(std::ostream &_ros) const = 0;
    virtual std::ostream& print(std::ostream &_ros, const EnumMap */*_penum_map*/) const = 0;
    virtual void reset(const std::string_view _name, const TypeMapBase *_ptype_map, ContextT &/*_rctx*/) = 0;
    virtual std::ostream& ostream() = 0;
    virtual std::istream& istream() const = 0;
};

namespace impl{

template <class Reflector, TypeGroupE TypeGroup>
class GroupNode;

template <class Reflector>
class GroupNode<Reflector, TypeGroupE::Basic>: public BaseNode<Reflector>{
protected:
    using BaseT = BaseNode<Reflector>;
    GroupNode(Reflector &_rreflector, const std::type_info * const _ptype_info):BaseT(_rreflector, TypeGroupE::Basic, _ptype_info){}
public:
    template <class T>
    T* as();
    
    std::ostream& print(std::ostream &_ros) const override = 0;
};

template <class Reflector>
class GroupNode<Reflector, TypeGroupE::Container>: public BaseNode<Reflector>{
protected:
    using ContextT = typename Reflector::ContextT;
    using BaseT = BaseNode<Reflector>;
    
    GroupNode(Reflector &_rreflector, const std::type_info * const _ptype_info):BaseT(_rreflector, TypeGroupE::Container, _ptype_info){}
public:
    template <class DispatchFunction>
    void for_each(DispatchFunction _dispatch_function, ContextT &_rctx){
        Reflector new_reflector{this->rreflector_.metadataFactory(), _dispatch_function};
        this->doForEach(new_reflector, _rctx);
    }
    
    template <class DispatchFunction>
    void for_each(DispatchFunction _dispatch_function, ContextT &_rctx)const{
        Reflector new_reflector{this->rreflector_.metadataFactory(), _dispatch_function};
        this->doForEach(new_reflector, _rctx);
    }
};

template <class Reflector>
class GroupNode<Reflector, TypeGroupE::Array>: public BaseNode<Reflector>{
protected:
    using ContextT = typename Reflector::ContextT;
    using BaseT = BaseNode<Reflector>;
    
    GroupNode(Reflector &_rreflector, const std::type_info * const _ptype_info):BaseT(_rreflector, TypeGroupE::Array, _ptype_info){}

public:
    template <class DispatchFunction>
    void for_each(DispatchFunction _dispatch_function, ContextT &_rctx){
        Reflector new_reflector{this->rreflector_.metadataFactory(), _dispatch_function, this->rreflector_.typeMap()};
        this->doForEach(new_reflector, _rctx);
    }
    
    template <class DispatchFunction>
    void for_each(DispatchFunction _dispatch_function, ContextT &_rctx)const{
        Reflector new_reflector{this->rreflector_.metadataFactory(), _dispatch_function, this->rreflector_.typeMap()};
        this->doForEach(new_reflector, _rctx);
    }
};

template <class Reflector>
class GroupNode<Reflector, TypeGroupE::Enum>: public BaseNode<Reflector>{
protected:
    using ContextT = typename Reflector::ContextT;
    using BaseT = BaseNode<Reflector>;
    
    GroupNode(Reflector &_rreflector, const std::type_info * const _ptype_info):BaseT(_rreflector, TypeGroupE::Enum, _ptype_info){}

public:
    std::ostream& print(std::ostream &_ros, const EnumMap */*_penum_map*/) const override = 0;
};

template <class Reflector>
class GroupNode<Reflector, TypeGroupE::SharedPtr>: public BaseNode<Reflector>{
protected:
    using ContextT = typename Reflector::ContextT;
    using BaseT = BaseNode<Reflector>;
    
    GroupNode(Reflector &_rreflector, const std::type_info * const _ptype_info):BaseT(_rreflector, TypeGroupE::SharedPtr, _ptype_info){}

public:
    
    bool isNull()const override = 0;
    
    template <class DispatchFunction>
    void visit(DispatchFunction _dispatch_function, const TypeMapBase *_ptype_map, ContextT &_rctx){
        Reflector new_reflector{this->rreflector_.metadataFactory(), _dispatch_function, this->rreflector_.typeMap()};
        this->doVisit(new_reflector, _ptype_map, _rctx);
    }
    
    template <class DispatchFunction>
    void visit(DispatchFunction _dispatch_function, const TypeMapBase *_ptype_map, ContextT &_rctx)const{
        Reflector new_reflector{this->rreflector_.metadataFactory(), _dispatch_function, this->rreflector_.typeMap()};
        this->doVisit(new_reflector, _ptype_map, _rctx);
    }
    
    void reset(const std::string_view _name, const TypeMapBase *_ptype_map, ContextT &_rctx) override = 0;
};

template <class Reflector>
class GroupNode<Reflector, TypeGroupE::UniquePtr>: public BaseNode<Reflector>{
protected:
    using ContextT = typename Reflector::ContextT;
    using BaseT = BaseNode<Reflector>;
    
    GroupNode(Reflector &_rreflector, const std::type_info * const _ptype_info):BaseT(_rreflector, TypeGroupE::UniquePtr, _ptype_info){}

public:
    bool isNull()const override = 0;
    
    template <class DispatchFunction>
    void visit(DispatchFunction _dispatch_function, const TypeMapBase *_ptype_map, ContextT &_rctx){
        Reflector new_reflector{this->rreflector_.metadataFactory(), _dispatch_function, this->rreflector_.typeMap()};
        this->doVisit(new_reflector, _ptype_map, _rctx);
    }
    template <class DispatchFunction>
    void visit(DispatchFunction _dispatch_function, const TypeMapBase *_ptype_map, ContextT &_rctx)const{
        Reflector new_reflector{this->rreflector_.metadataFactory(), _dispatch_function, this->rreflector_.typeMap()};
        this->doVisit(new_reflector, _ptype_map, _rctx);
    }
};

template <class Reflector>
class GroupNode<Reflector, TypeGroupE::Bitset>: public BaseNode<Reflector>{
protected:
    using BaseT = BaseNode<Reflector>;
    
    GroupNode(Reflector &_rreflector, const std::type_info * const _ptype_info):BaseT(_rreflector, TypeGroupE::Bitset, _ptype_info){}
public:
    virtual std::ostream& print(std::ostream &_ros)const{return _ros;}
};

template <class Reflector>
class GroupNode<Reflector, TypeGroupE::Unknwon>: public BaseNode<Reflector>{
protected:
    using BaseT = BaseNode<Reflector>;
    
    GroupNode(Reflector &_rreflector, const std::type_info * const _ptype_info):BaseT(_rreflector, TypeGroupE::Unknwon, _ptype_info){}
};

template <class Reflector>
class GroupNode<Reflector, TypeGroupE::Tuple>: public BaseNode<Reflector>{
protected:
    using ContextT = typename Reflector::ContextT;
    using BaseT = BaseNode<Reflector>;
    
    GroupNode(Reflector &_rreflector, const std::type_info * const _ptype_info):BaseT(_rreflector, TypeGroupE::Tuple, _ptype_info){}
public:
    template <class DispatchFunction>
    void for_each(DispatchFunction _dispatch_function, ContextT &_rctx){
        Reflector new_reflector{this->rreflector_.metadataFactory(), _dispatch_function, this->rreflector_.typeMap()};
        this->doForEach(new_reflector, _rctx);
    }
    
    template <class DispatchFunction>
    void for_each(DispatchFunction _dispatch_function, ContextT &_rctx)const{
        Reflector new_reflector{this->rreflector_.metadataFactory(), _dispatch_function, this->rreflector_.typeMap()};
        this->doForEach(new_reflector, _rctx);
    }
};

template <class Reflector>
class GroupNode<Reflector, TypeGroupE::IStream>: public BaseNode<Reflector>{
protected:
    using BaseT = BaseNode<Reflector>;
    
    GroupNode(Reflector &_rreflector, const std::type_info * const _ptype_info):BaseT(_rreflector, TypeGroupE::IStream, _ptype_info){}
    
    std::istream& istream() const override = 0;
};

template <class Reflector>
class GroupNode<Reflector, TypeGroupE::OStream>: public BaseNode<Reflector>{
protected:
    using BaseT = BaseNode<Reflector>;
    
    GroupNode(Reflector &_rreflector, const std::type_info * const _ptype_info):BaseT(_rreflector, TypeGroupE::OStream, _ptype_info){}
    
    std::ostream& ostream() override = 0;
};

template <class Reflector>
class GroupNode<Reflector, TypeGroupE::Structure>: public BaseNode<Reflector>{
protected:
    using ContextT = typename Reflector::ContextT;
    using BaseT = BaseNode<Reflector>;
    
    GroupNode(Reflector &_rreflector, const std::type_info * const _ptype_info):BaseT(_rreflector, TypeGroupE::Structure, _ptype_info){}
public:
    template <class DispatchFunction>
    void for_each(DispatchFunction _dispatch_function, ContextT &_rctx){
        Reflector new_reflector{this->rreflector_.metadataFactory(), _dispatch_function, this->rreflector_.typeMap()};
        this->doForEach(new_reflector, _rctx);
    }
    
    template <class DispatchFunction>
    void for_each(DispatchFunction _dispatch_function, ContextT &_rctx)const{
        Reflector new_reflector{this->rreflector_.metadataFactory(), _dispatch_function, this->rreflector_.typeMap()};
        this->doForEach(new_reflector, _rctx);
    }
};

template <class Reflector, class T>
class Node: public GroupNode<Reflector, type_group<std::remove_const_t<T>>()>{
    T &ref_;
    
    using ValueT = std::remove_const_t<T>;
public:
    using ContextT = typename Reflector::ContextT;
    using BaseT = GroupNode<Reflector, type_group<std::remove_const_t<T>>()>;
    
    Node(Reflector &_rreflector, T &_ref): BaseT(_rreflector, &typeid(T)), ref_(_ref){}
    
    T& reference(){
        return ref_;
    }
    const T& reference()const{
        return ref_;
    }

private:
    void doForEach(Reflector &_rreflector, ContextT &_rctx)override{
        if constexpr (type_group<ValueT>() == TypeGroupE::Array){
            const size_t size = std::extent_v<T>;
            for(size_t i = 0; i < size; ++i){
                _rreflector.add(ref_[i], _rctx, i, ""); 
            }
        }else if constexpr (type_group<ValueT>() == TypeGroupE::Container){
            size_t i = 0;
            for(auto &item: ref_){
                _rreflector.add(item, _rctx, i, "");
                ++i;
            }
        }else if constexpr (type_group<ValueT>() == TypeGroupE::Structure){
            solidReflectV1(_rreflector, ref_, _rctx);
        }else if constexpr (type_group<ValueT>() == TypeGroupE::Tuple){
            solidReflectV1(_rreflector, ref_, _rctx);
        }
        
    }
    
    void doForEach(Reflector &_rreflector, ContextT &_rctx)const override{
        if constexpr (type_group<ValueT>() == TypeGroupE::Array){
            const size_t size = std::extent_v<T>;
            for(size_t i = 0; i < size; ++i){
                _rreflector.add(ref_[i], _rctx, i, ""); 
            }
        }else if constexpr (type_group<ValueT>() == TypeGroupE::Container){
            size_t i = 0;
            for(auto &item: ref_){
                _rreflector.add(item, _rctx, i, "");
                ++i;
            }
        }else if constexpr (type_group<ValueT>() == TypeGroupE::Structure){
            solidReflectV1(_rreflector, ref_, _rctx);
        }else if constexpr (type_group<ValueT>() == TypeGroupE::Tuple){
            solidReflectV1(_rreflector, ref_, _rctx);
        }
    }
    
    void doVisit(Reflector &_rreflector, const TypeMapBase* _ptype_map, ContextT &_rctx)override{
        if constexpr (type_group<ValueT>() == TypeGroupE::SharedPtr || type_group<ValueT>() == TypeGroupE::UniquePtr){
            solid_check(ref_);
            if constexpr (
                type_group<typename T::element_type>() == TypeGroupE::Basic ||
                type_group<typename T::element_type>() == TypeGroupE::Container ||
                type_group<typename T::element_type>() == TypeGroupE::Enum ||
                type_group<typename T::element_type>() == TypeGroupE::Bitset
            ){
                _rreflector.add(*ref_, _rctx, 0, "");
            }else{
                solid_check(_ptype_map != nullptr);
                _ptype_map->reflect(_rreflector, *ref_.get(), _rctx);
            }
        }
    }
    
    void doVisit(Reflector &_rreflector, const TypeMapBase* _ptype_map, ContextT &_rctx)const override{
        if constexpr (type_group<ValueT>() == TypeGroupE::SharedPtr || type_group<ValueT>() == TypeGroupE::UniquePtr){
            solid_check(ref_);
            if constexpr (
                type_group<typename T::element_type>() == TypeGroupE::Basic ||
                type_group<typename T::element_type>() == TypeGroupE::Container ||
                type_group<typename T::element_type>() == TypeGroupE::Enum ||
                type_group<typename T::element_type>() == TypeGroupE::Bitset
            ){
                _rreflector.add(*ref_, _rctx, 0, "");
            }else{
                solid_check(_ptype_map != nullptr);
                _ptype_map->reflect(_rreflector, *ref_.get(), _rctx);
            }
        }
    }
    
    void reset(const std::string_view _name, const TypeMapBase *_ptype_map, ContextT &_rctx) override{
        if constexpr (!std::is_const_v<T> && (type_group<ValueT>() == TypeGroupE::SharedPtr || type_group<ValueT>() == TypeGroupE::UniquePtr)){
            solid_check(_ptype_map != nullptr);
            
            _ptype_map->create(ref_, 0, _name);
        }
    }
    
    std::ostream& print(std::ostream &_ros)const override{
        if constexpr (type_group<ValueT>() == TypeGroupE::Basic || type_group<ValueT>() == TypeGroupE::Bitset){
            _ros<<ref_;
        }
        return _ros;
    }
    
    std::ostream& print(std::ostream &_ros, const EnumMap *_penum_map)const override{
        if constexpr (type_group<ValueT>() == TypeGroupE::Enum){
            if(_penum_map){
                const char *name = _penum_map->get(ref_);
                if(name != nullptr){
                    _ros << name;
                }else{
                    _ros << static_cast<std::underlying_type_t<T>>(ref_);
                }
            }else{
                _ros << static_cast<std::underlying_type_t<T>>(ref_);
            }
        }
        return _ros;
    }
    bool isNull()const override{
        if constexpr (type_group<ValueT>() == TypeGroupE::SharedPtr || type_group<ValueT>() == TypeGroupE::UniquePtr){
            return !ref_;
        }
        return true;
    }
    
    std::ostream& ostream() override{
        if constexpr (type_group<ValueT>() == TypeGroupE::OStream){
            return ref_;
        }else{
            //NOTE: ugly but safe
            return std::cout;
        }
    }
    
    std::istream& istream() const override{
        if constexpr (type_group<ValueT>() == TypeGroupE::IStream){
            return *const_cast<std::istream*>(static_cast<const std::istream*>(&ref_));
        }else{
            //NOTE: ugly but safe
            return std::cin;
        }
    }
};

template <class Reflector>
template <class T>
T* GroupNode<Reflector, TypeGroupE::Basic>::as(){
    if(std::type_index(*BaseT::ptype_info_) == std::type_index(typeid(T))){
        return &static_cast<Node<Reflector, T>*>(this)->reference();
    }else{
        return nullptr;
    }
}

template <class MetadataVariant, class MetadataFactory, class Context = solid::EmptyType>
class Reflector{
    using DispatchFunctionT = std::function<void(BaseNode<Reflector>&, const MetadataVariant&, const size_t, const char*const, Context&)>;
    const MetadataFactory       &rmetadata_factory_;
    DispatchFunctionT           dispatch_function_;
    const TypeMapBase           *ptype_map_ = nullptr;
public:
    using ThisT = Reflector<MetadataVariant, MetadataFactory, Context>;
    using ContextT = Context;
    static constexpr bool is_const_reflector = false;
    
    
    template <class DispatchFunction>
    Reflector(
        const MetadataFactory &_rmetadata_factory,
        DispatchFunction _dispatch_function,
        const TypeMapBase *_ptype_map = nullptr
    ):rmetadata_factory_(_rmetadata_factory)
     , dispatch_function_(_dispatch_function)
     , ptype_map_(_ptype_map){}

    
    template <typename T, typename F>
    auto& add(T &_rt, Context &_rctx, const size_t _id, const char *const _name, F _f){
        static_assert(!std::is_invocable_v<T, ThisT &, Context&>, "Parameter should not be invocable");

        Node<ThisT, T> node{*this, _rt};
        auto meta = rmetadata_factory_(_rt, ptype_map_);
        _f(meta);
        dispatch_function_(node, meta, _id, _name, _rctx);
        return *this;
    }

    template <typename T>
    auto& add(T &_rt, Context &_rctx, const size_t _id, const char * const _name){
        static_assert(!std::is_invocable_v<T, ThisT &, Context&>, "Parameter should not be invocable");

        Node<ThisT, T> node{*this, _rt};
        auto meta = rmetadata_factory_(_rt, ptype_map_);
        dispatch_function_(node, meta, _id, _name, _rctx);
        return *this;
    }
    
    template <typename T>
    auto& add(T &&_rt, Context &_rctx){
        static_assert(std::is_invocable_v<T, ThisT &, Context&>, "Parameter should be invocable");
        std::invoke(_rt, *this, _rctx);
        return *this;
    }
    
    template <class , class MFactory, class T, class F, class C>
    friend void reflect(MFactory &&, T &, F &&, C&);
    
    const MetadataFactory& metadataFactory()const{
        return rmetadata_factory_;
    }
    
    const TypeMapBase* typeMap()const{
        return ptype_map_;
    }
    
};

template <class MetadataVariant, class MetadataFactory, class Context = solid::EmptyType>
class ConstReflector{
    using DispatchFunctionT = std::function<void(const BaseNode<ConstReflector>&, const MetadataVariant&, const size_t, const char*const, Context&)>;
    const MetadataFactory       &rmetadata_factory_;
    DispatchFunctionT           dispatch_function_;
    const TypeMapBase           *ptype_map_ = nullptr;
public:
    using ThisT = ConstReflector<MetadataVariant, MetadataFactory, Context>;
    using ContextT = Context;
    static constexpr bool is_const_reflector = true;
    
    template <class DispatchFunction>
    ConstReflector(
        const MetadataFactory &_rmetadata_factory,
        DispatchFunction _dispatch_function,
        const TypeMapBase *_ptype_map = nullptr
    ):rmetadata_factory_(_rmetadata_factory)
     , dispatch_function_(_dispatch_function)
     , ptype_map_(_ptype_map){}
     
     
    template <typename T, typename F>
    auto& add(const T &_rt, Context &_rctx, const size_t _id, const char *const _name, F _f){
        static_assert(!std::is_invocable_v<T, ThisT &, Context&>, "Parameter should not be invocable");

        Node<ThisT, const T> node{*this, _rt};
        auto meta = rmetadata_factory_(_rt, ptype_map_);
        _f(meta);
        dispatch_function_(node, meta, _id, _name, _rctx);
        return *this;
    }

    template <typename T>
    auto& add(const T &_rt, Context &_rctx, const size_t _id, const char * const _name){
        static_assert(!std::is_invocable_v<T, ThisT &, Context&>, "Parameter should not be invocable");

        Node<ThisT, const T> node{*this, _rt};
        auto meta = rmetadata_factory_(_rt, ptype_map_);
        dispatch_function_(node, meta, _id, _name, _rctx);
        return *this;
    }
    
    template <typename T>
    auto& add(T &&_rt, Context &_rctx){
        static_assert(std::is_invocable_v<T, ThisT &, Context&>, "Parameter should be invocable");
        std::invoke(_rt, *this, _rctx);
        return *this;
    }
    
    template <class , class MFactory, class T, class F, class C>
    friend void reflect(MFactory &&, T &, F &&, C&);
    
    const MetadataFactory& metadataFactory()const{
        return rmetadata_factory_;
    }
    
    const TypeMapBase* typeMap()const{
        return ptype_map_;
    }
};

}//namespace impl

template <class Reflector>
template <TypeGroupE TG>
auto *BaseNode<Reflector>::as(){
    using NodeT = impl::GroupNode<Reflector, TG>;
    if(this->typeGroup() == TG){
        return static_cast<NodeT*>(this);
    }else{
        return static_cast<NodeT*>(nullptr);
    }
}

template <class Reflector>
template <TypeGroupE TG>
auto *BaseNode<Reflector>::as()const{
    using NodeT = impl::GroupNode<Reflector, TG>;
    if(this->typeGroup() == TG){
        return static_cast<const NodeT*>(this);
    }else{
        return static_cast<const NodeT*>(nullptr);
    }
}

template <class MetadataVariant, class MetadataFactory, class T, class DispatchFunction, class Context = solid::EmptyType>
void reflect(MetadataFactory &&_meta_data_factory, T &_rt, DispatchFunction _dispatch_function, Context &_rctx){
    impl::Reflector<MetadataVariant, std::decay_t<MetadataFactory>, Context> reflector(_meta_data_factory, _dispatch_function);
    
    reflector.add(_rt, _rctx, 0, "root");
}

template <class MetadataVariant, class MetadataFactory, class T, class DispatchFunction, class Context = solid::EmptyType>
void const_reflect(MetadataFactory &&_meta_data_factory, T &_rt, DispatchFunction _dispatch_function, Context &_rctx){
    impl::ConstReflector<MetadataVariant, std::decay_t<MetadataFactory>, Context> reflector(_meta_data_factory, _dispatch_function);
    
    reflector.add(_rt, _rctx, 0, "root");
}

template <class MetadataVariant, class MetadataFactory, class T, class DispatchFunction, class Context = solid::EmptyType>
void reflect(MetadataFactory &&_meta_data_factory, T &_rt, DispatchFunction _dispatch_function, const TypeMapBase &_rtype_map, Context &_rctx){
    impl::Reflector<MetadataVariant, std::decay_t<MetadataFactory>, Context> reflector(_meta_data_factory, _dispatch_function, &_rtype_map);
    
    reflector.add(_rt, _rctx, 0, "root");
}

template <class MetadataVariant, class MetadataFactory, class T, class DispatchFunction, class Context = solid::EmptyType>
void const_reflect(MetadataFactory &&_meta_data_factory, T &_rt, DispatchFunction _dispatch_function, const TypeMapBase &_rtype_map, Context &_rctx){
    impl::ConstReflector<MetadataVariant, std::decay_t<MetadataFactory>, Context> reflector(_meta_data_factory, _dispatch_function, &_rtype_map);
    
    reflector.add(_rt, _rctx, 0, "root");
}

template <class MetadataVariant, class MetadataFactory, class Context = solid::EmptyType>
using ReflectorT = impl::Reflector<MetadataVariant, MetadataFactory, Context>;

template <class MetadataVariant, class MetadataFactory, class Context = solid::EmptyType>
using ConstReflectorT = impl::ConstReflector<MetadataVariant, MetadataFactory, Context>;

}//namespace v1
}//namespace reflection
}//namespace solid
