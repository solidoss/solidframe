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
#include <functional>
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
    virtual const void*        get() const                                                = 0;
    virtual void*              get()                                                      = 0;
    virtual FunctionValueBase* copyTo(void* _pd, const size_t _sz) const                  = 0;
    virtual FunctionValueBase* moveTo(void* _pd, const size_t _sz, const bool _uses_data) = 0;

    FunctionValueBase& operator=(const FunctionValueBase&) = delete;
};

//-----------------------------------------------------------------------------
//      FunctionValueInter<T>
//-----------------------------------------------------------------------------

template <class R, class... ArgTypes>
struct FunctionValueInter : FunctionValueBase {
    virtual R call(ArgTypes... args) = 0;
};

//-----------------------------------------------------------------------------
//      FunctionValue<T>
//-----------------------------------------------------------------------------
template <class T, bool CopyConstructible, class R, class... ArgTypes>
struct FunctionValue;

template <class T, class R, class... ArgTypes>
struct FunctionValue<T, true, R, ArgTypes...> : FunctionValueInter<R, ArgTypes...> {

    FunctionValue(T&& _rt)
        : value_(std::forward<T>(_rt))
    {
    }

    FunctionValue(const T& _rt)
        : value_(_rt)
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

    R call(ArgTypes... _args) override
    {
        return value_(std::forward<ArgTypes>(_args)...);
    }

    T value_;
};

template <class T, class R, class... ArgTypes>
struct FunctionValue<T, false, R, ArgTypes...> : FunctionValueInter<R, ArgTypes...> {

    explicit FunctionValue(T&& _rt)
        : value_(std::forward<T>(_rt))
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

    FunctionValue* copyTo(void* /*_pd*/, const size_t /*_sz*/) const override
    {
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

    R call(ArgTypes... _args) override
    {
        return value_(std::forward<ArgTypes>(_args)...);
    }

    T value_;
};

} //namespace impl

//constexpr size_t function_min_data_size = sizeof(impl::FunctionValue<void*, true, void>);

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

template <size_t Size>
struct FunctionData;

template <>
struct FunctionData<0> {
    inline const void* dataPtr() const
    {
        return nullptr;
    }

    inline void* dataPtr()
    {
        return nullptr;
    }
};

template <size_t Size>
struct FunctionData {
    union {
        char     data_[Size];
        uint64_t v_;
    } u_;

    inline const void* dataPtr() const
    {
        return reinterpret_cast<const void*>(&u_.data_[0]);
    }

    inline void* dataPtr()
    {
        return reinterpret_cast<void*>(&u_.data_[0]);
    }
};

//-----------------------------------------------------------------------------
//      Function<Size>
//-----------------------------------------------------------------------------

template <size_t DataSize, class>
class Function; // undefined

template <size_t DataSize, class R, class... ArgTypes>
class Function<DataSize, R(ArgTypes...)> : protected FunctionData<DataSize>, public FunctionBase {
    template <bool B>
    using bool_constant = std::integral_constant<bool, B>;

public:
    using ThisT = Function<DataSize, R(ArgTypes...)>;

    template <class T>
    using FunctionValueT      = impl::FunctionValue<T, std::is_copy_constructible<T>::value, R, ArgTypes...>;
    using FunctionValueInterT = impl::FunctionValueInter<R, ArgTypes...>;

    Function() {}

    explicit Function(std::nullptr_t) {}

    Function(const ThisT& _rany)
        : FunctionBase(doCopyFrom(_rany, this->dataPtr(), DataSize))
    {
        solid_check(_rany.empty() == this->empty(), "Copy Non Copyable");
    }

    Function(ThisT&& _rany)
        : FunctionBase(doMoveFrom(_rany, this->dataPtr(), DataSize, _rany.usesData()))
    {
        _rany.release(pvalue_);
    }

    template <class T>
    Function(const T& _t)
        : FunctionBase(
              do_allocate<typename std::remove_reference<T>::type>(
                  bool_constant<std::is_convertible<typename std::remove_reference<T>::type*, FunctionBase*>::value>(),
                  bool_constant<sizeof(FunctionValueT<typename std::remove_reference<T>::type>) <= DataSize>(),
                  _t))
    {
    }

    template <class T>
    Function(
        T&& _ut)
        : FunctionBase(
              do_allocate<typename std::remove_reference<T>::type>(
                  bool_constant<std::is_convertible<typename std::remove_reference<T>::type*, FunctionBase*>::value>(),
                  bool_constant<sizeof(FunctionValueT<typename std::remove_reference<T>::type>) <= DataSize>(),
                  std::forward<T>(_ut)))
    {
    }

    ~Function()
    {
        clear();
    }

    bool empty() const noexcept
    {
        return pvalue_ == nullptr;
    }

    explicit operator bool() const noexcept
    {
        return !empty();
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
            pvalue_ = doCopyFrom(_rany, this->dataPtr(), DataSize);
            solid_check(_rany.empty() == this->empty(), "Copy Non Copyable");
        }
        return *this;
    }

    ThisT& operator=(ThisT&& _rany)
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&_rany)) {
            clear();
            pvalue_ = doMoveFrom(_rany, this->dataPtr(), DataSize, _rany.usesData());
            _rany.release(pvalue_);
        }
        return *this;
    }

    ThisT& operator=(std::nullptr_t)
    {
        clear();
        return *this;
    }

    template <typename T>
    ThisT& operator=(const T& _rt)
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&_rt)) {
            clear();
            using RealT = typename std::remove_reference<T>::type;
            pvalue_     = do_allocate<RealT>(bool_constant<std::is_convertible<RealT*, FunctionBase*>::value>(), bool_constant<sizeof(FunctionValueT<RealT>) <= DataSize>(), _rt);
        }
        return *this;
    }

    template <class T>
    ThisT& operator=(T&& _ut)
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&_ut)) {
            clear();
            using RealT = typename std::remove_reference<T>::type;
            pvalue_     = do_allocate<RealT>(bool_constant<std::is_convertible<RealT*, FunctionBase*>::value>(), bool_constant<sizeof(FunctionValueT<RealT>) <= DataSize>(), std::forward<T>(_ut));
        }
        return *this;
    }

    bool usesData() const
    {
        return this->dataPtr() && reinterpret_cast<const void*>(pvalue_) == this->dataPtr();
    }

    R operator()(ArgTypes... args) const
    {
        if (!empty()) {
            return static_cast<FunctionValueInterT*>(pvalue_)->call(std::forward<ArgTypes>(args)...);
        } else {
            throw std::bad_function_call();
        }
    }

private:
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

    template <class T>
    void do_reinit(const TypeToType<T>&, T&& _arg)
    {
        clear();
        pvalue_ = do_allocate<T>(
            bool_constant<std::is_convertible<typename std::remove_reference<T>::type*, FunctionBase*>::value>(), bool_constant<sizeof(FunctionValueT<T>) <= DataSize>(), std::forward<T>(_arg));
    }

    template <class T>
    impl::FunctionValueBase* do_allocate(std::false_type /*_is_any*/, std::true_type /*_emplace_new*/, T&& _arg)
    {
        return new (this->dataPtr()) FunctionValueT<T>(std::forward<T>(_arg));
    }

    template <class T>
    impl::FunctionValueBase* do_allocate(std::false_type /*_is_any*/, std::true_type /*_emplace_new*/, const T& _arg)
    {
        return new (this->dataPtr()) FunctionValueT<T>(_arg);
    }

    template <class T>
    impl::FunctionValueBase* do_allocate(std::false_type /*_is_any*/, std::false_type /*_plain_new*/, T&& _arg)
    {
        return new FunctionValueT<T>(std::forward<T>(_arg));
    }

    template <class T>
    impl::FunctionValueBase* do_allocate(std::true_type /*_is_any*/, std::true_type /*_emplace_new*/, const T& _rany)
    {
        return doCopyFrom(_rany, this->dataPtr(), DataSize);
    }

    template <class T>
    impl::FunctionValueBase* do_allocate(std::true_type /*_is_any*/, std::false_type /*_plain_new*/, const T& _rany)
    {
        return doCopyFrom(_rany, this->dataPtr(), DataSize);
    }

    template <class T>
    impl::FunctionValueBase* do_allocate(std::true_type /*_is_any*/, std::true_type /*_emplace_new*/, T&& _uany)
    {
        impl::FunctionValueBase* rv = doMoveFrom(_uany, this->dataPtr(), DataSize, _uany.usesData());
        _uany.release(rv);
        return rv;
    }

    template <class T>
    impl::FunctionValueBase* do_allocate(std::true_type /*_is_any*/, std::false_type /*_plain_new*/, T&& _uany)
    {
        impl::FunctionValueBase* rv = doMoveFrom(_uany, this->dataPtr(), DataSize, _uany.usesData());
        _uany.release(rv);
        return rv;
    }
};

//-----------------------------------------------------------------------------

} //namespace solid
#ifdef SOLID_USE_STD_FUNCTION

#define solid_function_t(...) std::function<__VA_ARGS__>

#else

#ifndef SOLID_FUNCTION_STORAGE
#define SOLID_FUNCTION_STORAGE 24
#endif

#define solid_function_t(...) solid::Function<SOLID_FUNCTION_STORAGE, __VA_ARGS__>

#endif

#define solid_function_empty(f) (!f)
#define solid_function_clear(f) (f = nullptr)
