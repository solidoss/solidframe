// solid/utility/any.hpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_ANY_HPP
#define UTILITY_ANY_HPP

#include <cstddef>
#include <typeinfo>
#include <typeindex>
#include <utility>
#include <type_traits>
#include "solid/system/exception.hpp"


namespace solid{

namespace impl{
//-----------------------------------------------------------------------------
//      AnyValueBase
//-----------------------------------------------------------------------------
struct AnyValueBase{
    virtual ~AnyValueBase();
    virtual const void* get()const = 0;
    virtual void* get() = 0;
    virtual AnyValueBase* copyTo(void *_pd, const size_t _sz) const = 0;
    virtual AnyValueBase* moveTo(void *_pd, const size_t _sz, const bool _uses_data) = 0;

    AnyValueBase& operator=(const AnyValueBase&) = delete;
};

//-----------------------------------------------------------------------------
//      AnyValue<T>
//-----------------------------------------------------------------------------
template <class T, bool CopyConstructible = std::is_copy_constructible<T>::value>
struct AnyValue;

template <class T>
struct AnyValue<T, true>: AnyValueBase{

    template <class ...Args>
    explicit AnyValue(Args&& ..._args):value_(std::forward<Args>(_args)...){}


    const void* get()const override{
        return &value_;
    }

    void* get()override{
        return &value_;
    }

    AnyValueBase* copyTo(void *_pd, const size_t _sz) const override{
        if(sizeof(AnyValue<T>) <= _sz){
            return new(_pd) AnyValue<T>{value_};
        }else{
            return new AnyValue<T>{value_};
        }
    }

    AnyValueBase* moveTo(void *_pd, const size_t _sz, const bool _uses_data)override{
        if(sizeof(AnyValue<T>) <= _sz){
            return new(_pd) AnyValue<T>{std::move(value_)};
        }else if(not _uses_data){//the pointer was allocated
            return this;
        }else{
            return new AnyValue<T>{std::move(value_)};
        }
    }

    T   value_;
};

template <class T>
struct AnyValue<T, false>: AnyValueBase{
    //explicit AnyValue(const T &_rt){}

    //explicit AnyValue(T &&_rt):value_(std::move(_rt)){}

    template <class ...Args>
    explicit AnyValue(Args&& ..._args):value_(std::forward<Args>(_args)...){}

    const void* get()const override{
        return &value_;
    }

    void* get()override{
        return &value_;
    }

    AnyValueBase* copyTo(void *_pd, const size_t _sz) const override{
        SOLID_THROW("Copy on Non Copyable");
        return nullptr;
    }

    AnyValueBase* moveTo(void *_pd, const size_t _sz, const bool _uses_data)override{
        if(sizeof(AnyValue<T>) <= _sz){
            return new(_pd) AnyValue<T>{std::move(value_)};
        }else if(not _uses_data){//the pointer was allocated
            return this;
        }else{
            return new AnyValue<T>{std::move(value_)};
        }
    }

    T   value_;
};


}//namespace impl

//-----------------------------------------------------------------------------
//      AnyBase
//-----------------------------------------------------------------------------
class AnyBase{
public:
    static constexpr size_t min_data_size = sizeof(impl::AnyValue<void*>);
protected:

    AnyBase():pvalue_(nullptr){}

    void doMoveFrom(AnyBase &_ranybase, void *_pv, const size_t _sz, const bool _uses_data){
        if(_ranybase.pvalue_){
            pvalue_ = _ranybase.pvalue_->moveTo(_pv, _sz, _uses_data);
            if(pvalue_ == _ranybase.pvalue_){
                _ranybase.pvalue_ = nullptr;
            }
        }
    }

    void doCopyFrom(const AnyBase &_ranybase, void *_pv, const size_t _sz){
        if(_ranybase.pvalue_){
            pvalue_ = _ranybase.pvalue_->copyTo(_pv, _sz);
        }
    }

    impl::AnyValueBase  *pvalue_;
};


//-----------------------------------------------------------------------------
//      Any<Size>
//-----------------------------------------------------------------------------

template <size_t DataSize = 0>
class Any;

template <size_t DataSize>
class Any: protected AnyBase{
    template <size_t DS>
    friend class Any;
public:
    using ThisT = Any<DataSize>;


    Any(){}


    template <size_t DS>
    explicit Any(const Any<DS> &_rany){
        doCopyFrom(_rany, data_, DataSize);
    }

    template <size_t DS>
    explicit Any(Any<DS> &&_rany){
        doMoveFrom(_rany, data_, DataSize, _rany.usesData());
        _rany.clear();
    }

    Any(const ThisT &_rany){
        doCopyFrom(_rany, data_, DataSize);
    }

    Any(ThisT &&_rany){
        doMoveFrom(_rany, data_, DataSize, _rany.usesData());
        _rany.clear();
    }


    template <typename T>
    explicit Any(const T &_rt, bool _dummy){
        reset(_rt);
    }

    template <typename T>
    explicit Any(T &&_rt, bool _dummy){
        reset(std::move(_rt));
    }


    ~Any(){
        clear();
    }

    template <typename T>
    T* cast(){
        if(pvalue_){
            const std::type_info &req_type = typeid(impl::AnyValue<T>);
            const std::type_info &val_type = typeid(*pvalue_);
            if(std::type_index(req_type) == std::type_index(val_type)){
                return reinterpret_cast<T*>(pvalue_->get());
            }
        }
        return nullptr;
    }

    template <typename T>
    const T* cast()const{
        if(pvalue_){
            const std::type_info &req_type = typeid(impl::AnyValue<T>);
            const std::type_info &val_type = typeid(*pvalue_);
            if(std::type_index(req_type) == std::type_index(val_type)){
                return reinterpret_cast<const T*>(pvalue_->get());
            }
        }
        return nullptr;
    }

    template <typename T>
    T* constCast()const{
        return const_cast<T*>(cast<T>());
    }

    bool empty()const{
        return pvalue_ == nullptr;
    }

    void clear(){
        if(usesData()){
            pvalue_->~AnyValueBase();
        }else{
            delete pvalue_;
        }
        pvalue_ = nullptr;
    }

    ThisT& operator=(const ThisT &_rany){
        if(static_cast<const void*>(this) != static_cast<const void*>(&_rany)){
            clear();
            doCopyFrom(_rany, data_, DataSize);
        }
        return *this;
    }

    ThisT& operator=(ThisT &&_rany){
        if(static_cast<const void*>(this) != static_cast<const void*>(&_rany)){
            clear();
            doMoveFrom(_rany, data_, DataSize, _rany.usesData());
            _rany.clear();
        }
        return *this;
    }

    template <size_t DS>
    ThisT& operator=(const Any<DS> &_rany){
        if(static_cast<const void*>(this) != static_cast<const void*>(&_rany)){
            clear();
            doCopyFrom(_rany, data_, DataSize);
        }
        return *this;
    }

    template <size_t DS>
    ThisT& operator=(Any<DS> &&_rany){
        if(static_cast<const void*>(this) != static_cast<const void*>(&_rany)){
            clear();
            doMoveFrom(_rany, data_, DataSize, _rany.usesData());
            _rany.clear();
        }
        return *this;
    }

    template <typename T>
    void reset(const T &_rt){
        clear();

        pvalue_ = do_allocate<T>(BoolToType<sizeof(impl::AnyValue<T>) <= DataSize>(), _rt);
    }

    template <class T>
    void reset(T && _ut){
        clear();

        pvalue_ = do_allocate<T>(BoolToType<sizeof(impl::AnyValue<T>) <= DataSize>(), std::forward<T>(_ut));
    }

    bool usesData()const{
        return reinterpret_cast<const void*>(pvalue_) == reinterpret_cast<const void*>(data_);
    }
private:
    template <size_t DS, class T, class ...Args>
    friend Any<DS> make_any(Args&& ..._args);

    template <class T, class ...Args>
    void do_reinit(const TypeToType<T>&, Args&& ..._args){
        clear();
        pvalue_ = do_allocate<T>(BoolToType<sizeof(impl::AnyValue<T>) <= DataSize>(), std::forward<Args>(_args)...);
    }

    template <class T, class ...Args>
    impl::AnyValueBase* do_allocate(BoolToType<true> /*_emplace_new*/, Args&& ..._args){
        return new(data_) impl::AnyValue<T>(std::forward<Args>(_args)...);
    }

    template <class T, class ...Args>
    impl::AnyValueBase* do_allocate(BoolToType<false> /*_plain_new*/, Args&& ..._args){
        return new impl::AnyValue<T>(std::forward<Args>(_args)...);
    }
//     template <class T>
//     impl::AnyValueBase* do_allocate(const T &_rt,  BoolToType<true> /*_emplace_new*/){
//         return new(data_) impl::AnyValue<T>{_rt};
//     }
//
//     template <class T>
//     impl::AnyValueBase* do_allocate(const T &_rt,  BoolToType<false> /*_plain_new*/){
//         return new impl::AnyValue<T>{_rt};
//     }
private:
    char                data_[DataSize];
};

//-----------------------------------------------------------------------------

template <size_t DS, class T, class ...Args>
Any<DS> make_any(Args&& ..._args){
    Any<DS> a;
    a.do_reinit(TypeToType<T>(), std::forward<Args>(_args)...);
    return a;
}

//-----------------------------------------------------------------------------

}//namespace solid


#endif
