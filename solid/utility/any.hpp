// solid/utility/any.hpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com)
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
//      AnyValueBase
//-----------------------------------------------------------------------------
struct AnyValueBase {
    virtual ~AnyValueBase();
    virtual const void*   get() const                                                = 0;
    virtual void*         get()                                                      = 0;
    virtual AnyValueBase* copyTo(void* _pd, const size_t _sz) const                  = 0;
    virtual AnyValueBase* moveTo(void* _pd, const size_t _sz, const bool _uses_data) = 0;

    AnyValueBase& operator=(const AnyValueBase&) = delete;
};

//-----------------------------------------------------------------------------
//      AnyValue<T>
//-----------------------------------------------------------------------------
template <class T, bool CopyConstructible = std::is_copy_constructible<T>::value>
struct AnyValue;

template <class T>
struct AnyValue<T, true> : AnyValueBase {

    template <class... Args>
    explicit AnyValue(Args&&... _args)
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

    AnyValueBase* copyTo(void* _pd, const size_t _sz) const override
    {
        if (sizeof(AnyValue<T>) <= _sz) {
            return new (_pd) AnyValue<T>{value_};
        } else {
            return new AnyValue<T>{value_};
        }
    }

    AnyValueBase* moveTo(void* _pd, const size_t _sz, const bool _uses_data) override
    {
        if (sizeof(AnyValue<T>) <= _sz) {
            return new (_pd) AnyValue<T>{std::forward<T>(value_)};
        } else if (!_uses_data) { //the pointer was allocated
            return this;
        } else {
            return new AnyValue<T>{std::forward<T>(value_)};
        }
    }

    T value_;
};

template <class T>
struct AnyValue<T, false> : AnyValueBase {
    template <class... Args>
    explicit AnyValue(Args&&... _args)
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

    AnyValueBase* copyTo(void* _pd, const size_t _sz) const override
    {
        SOLID_THROW("Copy on Non Copyable");
        return nullptr;
    }

    AnyValueBase* moveTo(void* _pd, const size_t _sz, const bool _uses_data) override
    {
        if (sizeof(AnyValue<T>) <= _sz) {
            return new (_pd) AnyValue<T>{std::move(value_)};
        } else if (!_uses_data) { //the pointer was allocated
            return this;
        } else {
            return new AnyValue<T>{std::move(value_)};
        }
    }

    T value_;
};

} //namespace impl

constexpr size_t any_min_data_size = sizeof(impl::AnyValue<void*>);

template <class T>
constexpr size_t any_data_size()
{
    return sizeof(impl::AnyValue<T>);
}

//-----------------------------------------------------------------------------
//      AnyBase
//-----------------------------------------------------------------------------
class AnyBase {
protected:
    AnyBase(impl::AnyValueBase* _pvalue = nullptr)
        : pvalue_(_pvalue)
    {
    }

    static impl::AnyValueBase* doMoveFrom(AnyBase& _ranybase, void* _pv, const size_t _sz, const bool _uses_data)
    {
        impl::AnyValueBase* rv = nullptr;

        if (_ranybase.pvalue_) {
            rv = _ranybase.pvalue_->moveTo(_pv, _sz, _uses_data);
        }

        return rv;
    }

    static impl::AnyValueBase* doCopyFrom(const AnyBase& _ranybase, void* _pv, const size_t _sz)
    {
        if (_ranybase.pvalue_) {
            return _ranybase.pvalue_->copyTo(_pv, _sz);
        } else {
            return nullptr;
        }
    }

    impl::AnyValueBase* pvalue_;
};

//-----------------------------------------------------------------------------
//      Any<Size>
//-----------------------------------------------------------------------------

template <size_t DataSize = 0>
class Any;

template <size_t DataSize>
class Any : public AnyBase {
    template <size_t DS>
    friend class Any;

    static bool canCast(const impl::AnyValueBase& _rv, const std::type_info& _req_type)
    {
        return std::type_index(_req_type) == std::type_index(typeid(_rv));
    }

public:
    using ThisT = Any<DataSize>;

    Any() {}

    Any(const ThisT& _rany)
        : AnyBase(doCopyFrom(_rany, data_, DataSize))
    {
    }

    Any(ThisT&& _rany)
        : AnyBase(doMoveFrom(_rany, data_, DataSize, _rany.usesData()))
    {
        _rany.release(pvalue_);
    }

    template <class T>
    Any(
        const T& _rt)
        : AnyBase(
              do_allocate<T>(
                  BoolToType<std::is_convertible<typename std::remove_reference<T>::type*, AnyBase*>::value>(),
                  BoolToType<sizeof(impl::AnyValue<T>) <= DataSize>(),
                  _rt))
    {
    }

    template <class T>
    Any(
        T&& _ut)
        : AnyBase(
              do_allocate<T>(
                  BoolToType<std::is_convertible<typename std::remove_reference<T>::type*, AnyBase*>::value>(),
                  BoolToType<sizeof(impl::AnyValue<T>) <= DataSize>(),
                  std::forward<T>(_ut)))
    {
    }

    ~Any()
    {
        clear();
    }

    template <typename T>
    T* cast()
    {
        if (pvalue_) {
            const std::type_info& req_type = typeid(impl::AnyValue<T>);
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
            const std::type_info& req_type = typeid(impl::AnyValue<T>);
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
            pvalue_->~AnyValueBase();
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

            pvalue_ = do_allocate<T>(BoolToType<std::is_convertible<typename std::remove_reference<T>::type*, AnyBase*>::value>(), BoolToType<sizeof(impl::AnyValue<T>) <= DataSize>(), _rt);
        }
        return *this;
    }

    template <class T>
    ThisT& operator=(T&& _ut)
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&_ut)) {
            clear();

            pvalue_ = do_allocate<T>(BoolToType<std::is_convertible<typename std::remove_reference<T>::type*, AnyBase*>::value>(), BoolToType<sizeof(impl::AnyValue<T>) <= DataSize>(), std::forward<T>(_ut));
        }
        return *this;
    }

    bool usesData() const
    {
        return reinterpret_cast<const void*>(pvalue_) == reinterpret_cast<const void*>(data_);
    }

private:
    template <size_t DS, class T, class... Args>
    friend Any<DS> make_any(Args&&... _args);

    void release(impl::AnyValueBase* _pvalue)
    {
        if (_pvalue == pvalue_) {
            //moved pvalue_
        } else if (usesData()) {
            pvalue_->~AnyValueBase();
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
            BoolToType<std::is_convertible<typename std::remove_reference<T>::type*, AnyBase*>::value>(), BoolToType<sizeof(impl::AnyValue<T>) <= DataSize>(), std::forward<Args>(_args)...);
    }

    template <class T, class... Args>
    impl::AnyValueBase* do_allocate(BoolToType<false> /*_is_any*/, BoolToType<true> /*_emplace_new*/, Args&&... _args)
    {
        return new (data_) impl::AnyValue<T>(std::forward<Args>(_args)...);
    }

    template <class T, class... Args>
    impl::AnyValueBase* do_allocate(BoolToType<false> /*_is_any*/, BoolToType<false> /*_plain_new*/, Args&&... _args)
    {
        return new impl::AnyValue<T>(std::forward<Args>(_args)...);
    }

    template <class T>
    impl::AnyValueBase* do_allocate(BoolToType<true> /*_is_any*/, BoolToType<true> /*_emplace_new*/, const T& _rany)
    {
        return doCopyFrom(_rany, data_, DataSize);
    }

    template <class T>
    impl::AnyValueBase* do_allocate(BoolToType<true> /*_is_any*/, BoolToType<false> /*_plain_new*/, const T& _rany)
    {
        return doCopyFrom(_rany, data_, DataSize);
    }

    template <class T>
    impl::AnyValueBase* do_allocate(BoolToType<true> /*_is_any*/, BoolToType<true> /*_emplace_new*/, T&& _uany)
    {
        impl::AnyValueBase* rv = doMoveFrom(_uany, data_, DataSize, _uany.usesData());
        _uany.release(rv);
        return rv;
    }

    template <class T>
    impl::AnyValueBase* do_allocate(BoolToType<true> /*_is_any*/, BoolToType<false> /*_plain_new*/, T&& _uany)
    {
        impl::AnyValueBase* rv = doMoveFrom(_uany, data_, DataSize, _uany.usesData());
        _uany.release(rv);
        return rv;
    }

private:
    char data_[DataSize];
};

//-----------------------------------------------------------------------------

template <size_t DS, class T, class... Args>
Any<DS> make_any(Args&&... _args)
{
    Any<DS> a;
    a.do_reinit(TypeToType<T>(), std::forward<Args>(_args)...);
    return a;
}

//-----------------------------------------------------------------------------

} //namespace solid
