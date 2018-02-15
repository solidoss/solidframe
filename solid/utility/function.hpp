// solid/utility/function.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/exception.hpp"
#include <cstddef>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>

namespace solid {

namespace impl {
//-----------------------------------------------------------------------------
//      FunctionValueBase
//-----------------------------------------------------------------------------
struct FunctionValueBase {
    virtual ~FunctionValueBase();
    virtual const void*   get() const                                                = 0;
    virtual void*         get()                                                      = 0;
    virtual FunctionValueBase* copyTo(void* _pd, const size_t _sz) const                  = 0;
    virtual FunctionValueBase* moveTo(void* _pd, const size_t _sz, const bool _uses_data) = 0;

    FunctionValueBase& operator=(const FunctionValueBase&) = delete;
};

//-----------------------------------------------------------------------------
//      FunctionValue<T>
//-----------------------------------------------------------------------------
template <class T, bool CopyConstructible, class R, class... ArgTypes>
struct FunctionValue;

template <class T, class R, class... ArgTypes>
struct FunctionValue<T, true, R, ArgTypes...> : FunctionValueBase {

    template <class... Args>
    explicit FunctionValue(Args&&... _args)
        : value_(std::forward<Args>(_args)...)
    {
    }

    const void* get() const override
    {
        return &value_;
    }

    void* get() override
    {
        return &value_;
    }

    FunctionValue* copyTo(void* _pd, const size_t _sz) const override
    {
        using FunctionValueT = FunctionValue<T, std::is_copy_constructible<T>::value, R, ArgTypes...>;
        if (sizeof(FunctionValueT) <= _sz) {
            return new (_pd) FunctionValueT{value_};
        } else {
            return new FunctionValueT{value_};
        }
    }

    FunctionValue* moveTo(void* _pd, const size_t _sz, const bool _uses_data) override
    {
        using FunctionValueT = FunctionValue<T, std::is_copy_constructible<T>::value, R, ArgTypes...>;
        
        if (sizeof(FunctionValueT) <= _sz) {
            return new (_pd) FunctionValueT{std::forward<T>(value_)};
        } else if (!_uses_data) { //the pointer was allocated
            return this;
        } else {
            return new FunctionValueT{std::forward<T>(value_)};
        }
    }
    
    R call(ArgTypes... _args){
        return value_(std::forward<ArgTypes>(_args)...);
    }

    T value_;
};

template <class T, class R, class... ArgTypes>
struct FunctionValue<T, false, R, ArgTypes...> : FunctionValueBase {
    template <class... Args>
    explicit FunctionValue(Args&&... _args)
        : value_(std::forward<Args>(_args)...)
    {
    }

    const void* get() const override
    {
        return &value_;
    }

    void* get() override
    {
        return &value_;
    }

    FunctionValue* copyTo(void* _pd, const size_t _sz) const override
    {
        SOLID_THROW("Copy on Non Copyable");
        return nullptr;
    }

    FunctionValue* moveTo(void* _pd, const size_t _sz, const bool _uses_data) override
    {
        using FunctionValueT = FunctionValue<T, std::is_copy_constructible<T>::value, R, ArgTypes...>;
        if (sizeof(FunctionValueT) <= _sz) {
            return new (_pd) FunctionValueT{std::move(value_)};
        } else if (!_uses_data) { //the pointer was allocated
            return this;
        } else {
            return new FunctionValueT{std::move(value_)};
        }
    }
    
    R call(ArgTypes... _args){
        return value_(std::forward<ArgTypes>(_args)...);
    }

    T value_;
};

} //namespace impl

constexpr size_t function_min_data_size = sizeof(impl::FunctionValue<void*, true, void>);

// template <class T>
// constexpr size_t function_data_size()
// {
//     return sizeof(impl::FunctionValue<T>);
// }

//-----------------------------------------------------------------------------
//      FunctionBase
//-----------------------------------------------------------------------------
class FunctionBase {
protected:
    FunctionBase(impl::FunctionValueBase* _pvalue = nullptr)
        : pvalue_(_pvalue)
    {
    }

    static impl::FunctionValueBase* doMoveFrom(FunctionBase& _ranybase, void* _pv, const size_t _sz, const bool _uses_data)
    {
        impl::FunctionValueBase* rv = nullptr;

        if (_ranybase.pvalue_) {
            rv = _ranybase.pvalue_->moveTo(_pv, _sz, _uses_data);
        }

        return rv;
    }

    static impl::FunctionValueBase* doCopyFrom(const FunctionBase& _ranybase, void* _pv, const size_t _sz)
    {
        if (_ranybase.pvalue_) {
            return _ranybase.pvalue_->copyTo(_pv, _sz);
        } else {
            return nullptr;
        }
    }

    impl::FunctionValueBase* pvalue_;
};

//-----------------------------------------------------------------------------
//      Function<Size>
//-----------------------------------------------------------------------------

template<size_t DataSize, class> class Function; // undefined

template<size_t DataSize, class R, class... ArgTypes>
class Function<DataSize, R(ArgTypes...)>: public FunctionBase{
    //template <size_t DS>
    //friend class Function<DS, R, ArgTypes...>;

    static bool canCast(const impl::FunctionValueBase& _rv, const std::type_info& _req_type)
    {
        return std::type_index(_req_type) == std::type_index(typeid(_rv));
    }

public:
    using ThisT = Function<DataSize, R, ArgTypes...>;
    template <class T>
    using FunctionValueT = impl::FunctionValue<T, std::is_copy_constructible<T>::value, R, ArgTypes...>;
    Function() {}

    Function(const ThisT& _rany)
        : FunctionBase(doCopyFrom(_rany, data_, DataSize))
    {
    }

    Function(ThisT&& _rany)
        : FunctionBase(doMoveFrom(_rany, data_, DataSize, _rany.usesData()))
    {
        _rany.release(pvalue_);
    }

    template <class T>
    Function(
        const T& _rt)
        : FunctionBase(
              do_allocate<T>(
                  BoolToType<std::is_convertible<typename std::remove_reference<T>::type*, FunctionBase*>::value>(),
                  BoolToType<sizeof(FunctionValueT<T>) <= DataSize>(),
                  _rt))
    {
    }

    template <class T>
    Function(
        T&& _ut)
        : FunctionBase(
              do_allocate<T>(
                  BoolToType<std::is_convertible<typename std::remove_reference<T>::type*, FunctionBase*>::value>(),
                  BoolToType<sizeof(FunctionValueT<T>) <= DataSize>(),
                  std::forward<T>(_ut)))
    {
    }

    ~Function()
    {
        clear();
    }

    template <typename T>
    T* cast()
    {
        if (pvalue_) {
            const std::type_info& req_type = typeid(FunctionValueT<T>);
            if (canCast(*pvalue_, req_type)) {
                return reinterpret_cast<T*>(pvalue_->get());
            }
        }
        return nullptr;
    }

    template <typename T>
    const T* cast() const
    {
        if (pvalue_) {
            const std::type_info& req_type = typeid(FunctionValueT<T>);
            if (canCast(*pvalue_, req_type)) {
                return reinterpret_cast<const T*>(pvalue_->get());
            }
        }
        return nullptr;
    }

    template <typename T>
    T* constCast() const
    {
        return const_cast<T*>(cast<T>());
    }

    bool empty() const
    {
        return pvalue_ == nullptr;
    }

    void clear()
    {
        if (usesData()) {
            pvalue_->~FunctionValueBase();
        } else {
            delete pvalue_;
        }
        pvalue_ = nullptr;
    }

    ThisT& operator=(const ThisT& _rany)
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&_rany)) {
            clear();
            pvalue_ = doCopyFrom(_rany, data_, DataSize);
        }
        return *this;
    }

    ThisT& operator=(ThisT&& _rany)
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&_rany)) {
            clear();
            pvalue_ = doMoveFrom(_rany, data_, DataSize, _rany.usesData());
            _rany.release(pvalue_);
        }
        return *this;
    }

    template <typename T>
    ThisT& operator=(const T& _rt)
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&_rt)) {
            clear();

            pvalue_ = do_allocate<T>(BoolToType<std::is_convertible<typename std::remove_reference<T>::type*, FunctionBase*>::value>(), BoolToType<sizeof(FunctionValueT<T>) <= DataSize>(), _rt);
        }
        return *this;
    }

    template <class T>
    ThisT& operator=(T&& _ut)
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&_ut)) {
            clear();

            pvalue_ = do_allocate<T>(BoolToType<std::is_convertible<typename std::remove_reference<T>::type*, FunctionBase*>::value>(), BoolToType<sizeof(FunctionValueT<T>) <= DataSize>(), std::forward<T>(_ut));
        }
        return *this;
    }

    bool usesData() const
    {
        return reinterpret_cast<const void*>(pvalue_) == reinterpret_cast<const void*>(data_);
    }
    
    R operator()(ArgTypes... args) const{
        //return (*any_.cast<StubBase>())(std::forward<ArgTypes>(args)...);
    }

private:
    //template <size_t DS, class T, class... Args>
    //friend Function<DS> make_any(Args&&... _args);

    void release(impl::FunctionValueBase* _pvalue)
    {
        if (_pvalue == pvalue_) {
            //moved pvalue_
        } else if (usesData()) {
            pvalue_->~FunctionValueBase();
        } else {
            delete pvalue_;
        }
        pvalue_ = nullptr;
    }

    template <class T, class... Args>
    void do_reinit(const TypeToType<T>&, Args&&... _args)
    {
        clear();
        pvalue_ = do_allocate<T>(
            BoolToType<std::is_convertible<typename std::remove_reference<T>::type*, FunctionBase*>::value>(), BoolToType<sizeof(FunctionValueT<T>) <= DataSize>(), std::forward<Args>(_args)...);
    }

    template <class T, class... Args>
    impl::FunctionValueBase* do_allocate(BoolToType<false> /*_is_any*/, BoolToType<true> /*_emplace_new*/, Args&&... _args)
    {
        return new (data_) FunctionValueT<T>(std::forward<Args>(_args)...);
    }

    template <class T, class... Args>
    impl::FunctionValueBase* do_allocate(BoolToType<false> /*_is_any*/, BoolToType<false> /*_plain_new*/, Args&&... _args)
    {
        return new FunctionValueT<T>(std::forward<Args>(_args)...);
    }

    template <class T>
    impl::FunctionValueBase* do_allocate(BoolToType<true> /*_is_any*/, BoolToType<true> /*_emplace_new*/, const T& _rany)
    {
        return doCopyFrom(_rany, data_, DataSize);
    }

    template <class T>
    impl::FunctionValueBase* do_allocate(BoolToType<true> /*_is_any*/, BoolToType<false> /*_plain_new*/, const T& _rany)
    {
        return doCopyFrom(_rany, data_, DataSize);
    }

    template <class T>
    impl::FunctionValueBase* do_allocate(BoolToType<true> /*_is_any*/, BoolToType<true> /*_emplace_new*/, T&& _uany)
    {
        impl::FunctionValueBase* rv = doMoveFrom(_uany, data_, DataSize, _uany.usesData());
        _uany.release(rv);
        return rv;
    }

    template <class T>
    impl::FunctionValueBase* do_allocate(BoolToType<true> /*_is_any*/, BoolToType<false> /*_plain_new*/, T&& _uany)
    {
        impl::FunctionValueBase* rv = doMoveFrom(_uany, data_, DataSize, _uany.usesData());
        _uany.release(rv);
        return rv;
    }

private:
    char data_[DataSize];
};

//-----------------------------------------------------------------------------

// template <size_t DS, class T, class... Args>
// Function<DS> make_any(Args&&... _args)
// {
//     Function<DS> a;
//     a.do_reinit(TypeToType<T>(), std::forward<Args>(_args)...);
//     return a;
// }

//-----------------------------------------------------------------------------

} //namespace solid


#if 0
#pragma once

#include "solid/utility/any.hpp"
#include "solid/utility/common.hpp"


namespace solid{

template<size_t DataSize, class> class Function; // undefined

template<size_t DataSize, class R, class... ArgTypes>
class Function<DataSize, R(ArgTypes...)>{
    
    struct StubBase{
        virtual ~StubBase(){}
        virtual R operator()(ArgTypes... args) = 0;
    };
    
    template <class T>
    struct Stub: StubBase{
        T   t_;
        
        Stub(T &&_ut):t_(std::move(_ut)){}
        
        R operator()(ArgTypes... args) override{
            return t_(std::forward<ArgTypes>(args)...);
        }
    };
public:
    template <class T>
    Function(T &&_ut):any_(Stub<T>(std::move(_ut))){
        
    }
    
    
    explicit operator bool() const noexcept{
        return not any_.empty();
    }
    
    R operator()(ArgTypes... args) const{
        return (*any_.cast<StubBase>())(std::forward<ArgTypes>(args)...);
    }
private:
    Function<DataSize>   any_;
};

}//namespace solid
#endif
