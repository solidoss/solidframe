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
#include "solid/system/log.hpp"
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

    AnyValueBase* copyTo(void* /*_pd*/, const size_t /*_sz*/) const override
    {
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

template <size_t Size>
struct AnyData;

template <>
struct AnyData<0> {
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
struct AnyData {
    char data_[Size];

    inline const void* dataPtr() const
    {
        return reinterpret_cast<const void*>(&data_[0]);
    }

    inline void* dataPtr()
    {
        return reinterpret_cast<void*>(&data_[0]);
    }
};

//-----------------------------------------------------------------------------
//      Any<Size>
//-----------------------------------------------------------------------------

template <size_t DataSize = 0>
class Any;

template <size_t DataSize>
class Any : protected AnyData<DataSize>, public AnyBase {
    template <size_t DS>
    friend class Any;

    static bool canCast(const impl::AnyValueBase& _rv, const std::type_info& _req_type)
    {
        return std::type_index(_req_type) == std::type_index(typeid(_rv));
    }

    template <bool B>
    using bool_constant = std::integral_constant<bool, B>;

public:
    using ThisT = Any<DataSize>;

    Any() {}

    Any(const ThisT& _rany)
        : AnyBase(doCopyFrom(_rany, this->dataPtr(), DataSize))
    {
        solid_check_log(_rany.empty() == this->empty(), generic_logger, "Copy Non Copyable");
    }

    Any(ThisT&& _rany)
        : AnyBase(doMoveFrom(_rany, this->dataPtr(), DataSize, _rany.usesData()))
    {
        _rany.release(pvalue_);
    }

    template <class T, typename = typename std::enable_if<!std::is_same<Any, typename std::decay<T>::type>::value, void>::type>
    explicit Any(
        const T& _rt)
        : AnyBase(
              do_allocate<typename std::remove_reference<T>::type>(
                  bool_constant<std::is_convertible<typename std::remove_reference<T>::type*, AnyBase*>::value>(),
                  bool_constant<sizeof(impl::AnyValue<typename std::remove_reference<T>::type>) <= DataSize>(),
                  _rt))
    {
    }

    template <class T, typename = typename std::enable_if<!std::is_same<Any, typename std::decay<T>::type>::value, void>::type>
    explicit Any(
        T&& _ut)
        : AnyBase(
              do_allocate<typename std::remove_reference<T>::type>(
                  bool_constant<std::is_convertible<typename std::remove_reference<T>::type*, AnyBase*>::value>(),
                  bool_constant<sizeof(impl::AnyValue<typename std::remove_reference<T>::type>) <= DataSize>(),
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
            pvalue_->~AnyValueBase();
        } else {
            delete pvalue_;
        }
        pvalue_ = nullptr;
    }

    Any& operator=(const Any& _rany)
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&_rany)) {
            clear();
            pvalue_ = doCopyFrom(_rany, this->dataPtr(), DataSize);
            solid_check_log(_rany.empty() == this->empty(), generic_logger, "Copy Non Copyable");
        }
        return *this;
    }

    Any& operator=(Any&& _rany)
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&_rany)) {
            clear();
            pvalue_ = doMoveFrom(_rany, this->dataPtr(), DataSize, _rany.usesData());
            _rany.release(pvalue_);
        }
        return *this;
    }

    template <typename T>
    typename std::enable_if<!std::is_same<ThisT, T>::value, ThisT&>::type
    operator=(const T& _rt)
    {
        clear();
        using RealT = typename std::decay<T>::type;
        pvalue_     = do_allocate<RealT>(bool_constant<std::is_convertible<RealT*, AnyBase*>::value>(), bool_constant<sizeof(impl::AnyValue<RealT>) <= DataSize>(), _rt);
        return *this;
    }

    template <class T>
    typename std::enable_if<!std::is_same<ThisT, T>::value, ThisT&>::type
    operator=(T&& _ut)
    {
        clear();
        using RealT = typename std::decay<T>::type;
        pvalue_     = do_allocate<RealT>(bool_constant<std::is_convertible<RealT*, AnyBase*>::value>(), bool_constant<sizeof(impl::AnyValue<RealT>) <= DataSize>(), std::forward<T>(_ut));
        return *this;
    }

    bool usesData() const
    {
        return this->dataPtr() && reinterpret_cast<const void*>(pvalue_) == this->dataPtr();
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
        using RealT = typename std::decay<T>::type;
        pvalue_     = do_allocate<RealT>(
            bool_constant<std::is_convertible<RealT*, AnyBase*>::value>(), bool_constant<sizeof(impl::AnyValue<RealT>) <= DataSize>(), std::forward<Args>(_args)...);
    }

    template <class T, class... Args>
    impl::AnyValueBase* do_allocate(std::false_type /*_is_any*/, std::true_type /*_emplace_new*/, Args&&... _args)
    {
        return new (this->dataPtr()) impl::AnyValue<T>(std::forward<Args>(_args)...);
    }

    template <class T, class... Args>
    impl::AnyValueBase* do_allocate(std::false_type /*_is_any*/, std::false_type /*_plain_new*/, Args&&... _args)
    {
        return new impl::AnyValue<T>(std::forward<Args>(_args)...);
    }

    template <class T>
    impl::AnyValueBase* do_allocate(std::true_type /*_is_any*/, std::true_type /*_emplace_new*/, const T& _rany)
    {
        return doCopyFrom(_rany, this->dataPtr(), DataSize);
    }

    template <class T>
    impl::AnyValueBase* do_allocate(std::true_type /*_is_any*/, std::false_type /*_plain_new*/, const T& _rany)
    {
        return doCopyFrom(_rany, this->dataPtr(), DataSize);
    }

    template <class T>
    impl::AnyValueBase* do_allocate(std::true_type /*_is_any*/, std::true_type /*_emplace_new*/, T&& _uany)
    {
        impl::AnyValueBase* rv = doMoveFrom(_uany, this->dataPtr(), DataSize, _uany.usesData());
        _uany.release(rv);
        return rv;
    }

    template <class T>
    impl::AnyValueBase* do_allocate(std::true_type /*_is_any*/, std::false_type /*_plain_new*/, T&& _uany)
    {
        impl::AnyValueBase* rv = doMoveFrom(_uany, this->dataPtr(), DataSize, _uany.usesData());
        _uany.release(rv);
        return rv;
    }
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
