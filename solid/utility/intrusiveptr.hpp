// solid/utility/intrusiveptr.hpp
//
// Copyright (c) 2024 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#pragma once

#include <atomic>
#include <cstddef>

namespace solid {

struct IntrusiveThreadSafePolicy;

class IntrusiveThreadSafeBase {
    friend struct IntrusiveThreadSafePolicy;
    mutable std::atomic_size_t use_count_{0};

protected:
    auto useCount() const
    {
        return use_count_.load(std::memory_order_relaxed);
    }
};

struct IntrusiveThreadSafePolicy {

    static void acquire(const IntrusiveThreadSafeBase& _r) noexcept
    {
        ++_r.use_count_;
    }
    static bool release(const IntrusiveThreadSafeBase& _r) noexcept
    {
        return _r.use_count_.fetch_sub(1) == 1;
    }

    template <class T>
    static void destroy(const T& _r)
    {
        static_assert(!std::is_same_v<T, IntrusiveThreadSafeBase>, "cannot destroy object from non virtual base class IntrusiveThreadSafeBase");
        delete &_r;
    }

    static auto use_count(const IntrusiveThreadSafeBase& _r) noexcept
    {
        return _r.use_count_.load(std::memory_order_relaxed);
    }

    template <class T, class... Args>
    static T* create(Args&&... _args)
    {
        return new T(std::forward<Args>(_args)...);
    }
};

template <typename T>
struct intrusive_policy_dispatch;

#ifdef __cpp_concepts
template <typename T>
concept DefaultIntrusiveC = std::is_base_of_v<IntrusiveThreadSafeBase, T>;

template <DefaultIntrusiveC T>
struct intrusive_policy_dispatch<T> {
    using type = IntrusiveThreadSafePolicy;
};

template <typename T>
using intrusive_policy_dispatch_t = typename intrusive_policy_dispatch<T>::type;
#endif

namespace impl {
template <class T, class PolicyT = intrusive_policy_dispatch_t<T>>
bool intrusive_ptr_release(const T& _r)
{
    return PolicyT::release(_r);
}

template <class T, class PolicyT = intrusive_policy_dispatch_t<T>>
void intrusive_ptr_destroy(const T& _r)
{
    return PolicyT::destroy(_r);
}

template <class T, class PolicyT = intrusive_policy_dispatch_t<T>>
void intrusive_ptr_acquire(const T& _r)
{
    PolicyT::acquire(_r);
}

template <class T, class PolicyT = intrusive_policy_dispatch_t<T>>
size_t intrusive_ptr_use_count(const T& _r)
{
    return PolicyT::use_count(_r);
}

template <class T, class... Args, class PolicyT = intrusive_policy_dispatch_t<T>>
T* intrusive_ptr_create(Args&&... _args)
{
    return PolicyT::template create<T>(std::forward<Args>(_args)...);
}

//

template <class T>
class IntrusivePtrBase {
    using ThisT = IntrusivePtrBase<T>;
    template <class TT>
    friend class IntrusivePtrBase;

protected:
    T* ptr_ = nullptr;

protected:
    IntrusivePtrBase() = default;

    IntrusivePtrBase(const IntrusivePtrBase& _other)
        : ptr_(_other.ptr_)
    {
        if (ptr_) {
            intrusive_ptr_acquire(*ptr_);
        }
    }

    IntrusivePtrBase(IntrusivePtrBase&& _other)
        : ptr_(_other.detach())
    {
    }

    template <class TT>
    IntrusivePtrBase(const IntrusivePtrBase<TT>& _other)
        : ptr_(static_cast<T*>(_other.ptr_))
    {
        if (ptr_) {
            intrusive_ptr_acquire(*ptr_);
        }
    }

    template <class TT>
    IntrusivePtrBase(IntrusivePtrBase<TT>&& _other)
        : ptr_(static_cast<TT*>(_other.detach()))
    {
    }

    ~IntrusivePtrBase()
    {
        if (ptr_ && intrusive_ptr_release(*ptr_)) {
            intrusive_ptr_destroy(*ptr_);
        }
    }

    void doCopy(const IntrusivePtrBase& _other) noexcept
    {
        IntrusivePtrBase{_other}.swap(*this);
    }
    void doMove(IntrusivePtrBase&& _other) noexcept
    {
        IntrusivePtrBase{std::move(_other)}.swap(*this);
    }

    template <class TT>
    void doCopy(const IntrusivePtrBase<TT>& _other) noexcept
    {
        IntrusivePtrBase{_other}.swap(*this);
    }

    template <class TT>
    void doMove(IntrusivePtrBase<TT>&& _other) noexcept
    {
        IntrusivePtrBase{std::move(_other)}.swap(*this);
    }

    IntrusivePtrBase(T* _ptr)
        : ptr_(_ptr)
    {
        if (_ptr) {
            intrusive_ptr_acquire(*_ptr);
        }
    }

    IntrusivePtrBase(T* _ptr, const bool _do_acquire)
        : ptr_(_ptr)
    {
        if (_ptr && _do_acquire) {
            intrusive_ptr_acquire(*_ptr);
        }
    }

    T* detach() noexcept
    {
        T* ret = ptr_;
        ptr_   = nullptr;
        return ret;
    }

    void swap(IntrusivePtrBase& _other) noexcept
    {
        T* tmp      = ptr_;
        ptr_        = _other.ptr_;
        _other.ptr_ = tmp;
    }

public:
    using element_type = T;

    explicit operator bool() const noexcept
    {
        return ptr_ != nullptr;
    }

    void reset()
    {
        if (ptr_) {
            if (intrusive_ptr_release(*ptr_)) {
                intrusive_ptr_destroy(*ptr_);
            }
            ptr_ = nullptr;
        }
    }

    size_t useCount() const noexcept
    {
        if (ptr_ != nullptr) [[likely]] {
            return intrusive_ptr_use_count(*ptr_);
        }
        return 0;
    }
};

} // namespace impl

template <class T>
class IntrusivePtr : public impl::IntrusivePtrBase<T> {
    using BaseT = impl::IntrusivePtrBase<T>;
    using ThisT = IntrusivePtr<T>;
    template <class TT>
    friend class IntrusivePtr;

public:
    IntrusivePtr() = default;

    IntrusivePtr(T* _ptr, const bool _do_acquire)
        : BaseT(_ptr, _do_acquire)
    {
    }

    IntrusivePtr(const IntrusivePtr& _other)
        : BaseT(_other)
    {
    }

    IntrusivePtr(IntrusivePtr&& _other)
        : BaseT(std::move(_other))
    {
    }

    template <class TT>
    IntrusivePtr(const IntrusivePtr<TT>& _other)
        : BaseT(_other)
    {
    }

    template <class TT>
    IntrusivePtr(IntrusivePtr<TT>&& _other)
        : BaseT(std::move(_other))
    {
    }

    ~IntrusivePtr() = default;

    IntrusivePtr& operator=(const IntrusivePtr& _other) noexcept
    {
        BaseT::doCopy(_other);
        return *this;
    }

    IntrusivePtr& operator=(IntrusivePtr&& _other) noexcept
    {
        BaseT::doMove(std::move(_other));
        return *this;
    }

    template <class TT>
    IntrusivePtr& operator=(const IntrusivePtr<TT>& _other) noexcept
    {
        BaseT::doCopy(_other);
        return *this;
    }

    template <class TT>
    IntrusivePtr& operator=(IntrusivePtr<TT>&& _other) noexcept
    {
        BaseT::doMove(std::move(_other));
        return *this;
    }

    T* get() const noexcept
    {
        return BaseT::ptr_;
    }

    T& operator*() const noexcept
    {
        return *BaseT::ptr_;
    }

    T* operator->() const noexcept
    {
        return BaseT::ptr_;
    }

private:
    template <class TT, class... Args>
    friend IntrusivePtr<TT> make_intrusive(Args&&... _args);
    template <class T1, class T2>
    friend IntrusivePtr<T1> static_pointer_cast(const IntrusivePtr<T2>& _rp) noexcept;
    template <class T1, class T2>
    friend IntrusivePtr<T1> dynamic_pointer_cast(const IntrusivePtr<T2>& _rp) noexcept;
    template <class T1, class T2>
    friend IntrusivePtr<T1> static_pointer_cast(IntrusivePtr<T2>&& _rp) noexcept;
    template <class T1, class T2>
    friend IntrusivePtr<T1> dynamic_pointer_cast(IntrusivePtr<T2>&& _rp) noexcept;

    IntrusivePtr(T* _ptr)
        : BaseT(_ptr)
    {
    }
};

//-----------------------------------------------------------------------------
// MutableIntrusivePtr
//-----------------------------------------------------------------------------
template <class T>
class ConstIntrusivePtr;

template <class T>
class MutableIntrusivePtr : public impl::IntrusivePtrBase<T> {
    using BaseT = impl::IntrusivePtrBase<T>;
    using ThisT = MutableIntrusivePtr<T>;
    template <class TT>
    friend class MutableIntrusivePtr;
    template <class TT>
    friend class ConstIntrusivePtr;

    MutableIntrusivePtr(IntrusivePtr<T>&& _other)
        : BaseT(std::move(_other))
    {
    }

public:
    MutableIntrusivePtr() = default;

    MutableIntrusivePtr(T* _ptr, const bool _do_acquire)
        : BaseT(_ptr, _do_acquire)
    {
    }

    MutableIntrusivePtr(const MutableIntrusivePtr& _other) = delete;

    template <class TT>
    MutableIntrusivePtr(const MutableIntrusivePtr<TT>& _other) = delete;

    MutableIntrusivePtr(MutableIntrusivePtr&& _other)
        : BaseT(std::move(_other))
    {
    }
    template <class TT>
    MutableIntrusivePtr(MutableIntrusivePtr<TT>&& _other)
        : BaseT(std::move(_other))
    {
    }

    ~MutableIntrusivePtr() = default;

    MutableIntrusivePtr& operator=(const MutableIntrusivePtr& _other) noexcept = delete;

    template <class TT>
    MutableIntrusivePtr& operator=(const MutableIntrusivePtr<TT>& _other) noexcept = delete;

    MutableIntrusivePtr& operator=(MutableIntrusivePtr&& _other) noexcept
    {
        BaseT::doMove(std::move(_other));
        return *this;
    }
    template <class TT>
    MutableIntrusivePtr& operator=(MutableIntrusivePtr<TT>&& _other) noexcept
    {
        BaseT::doMove(std::move(_other));
        return *this;
    }

    T* get() const noexcept
    {
        return BaseT::ptr_;
    }

    T& operator*() const noexcept
    {
        return *BaseT::ptr_;
    }

    T* operator->() const noexcept
    {
        return BaseT::ptr_;
    }

private:
    template <class TT, class... Args>
    friend MutableIntrusivePtr<TT> make_mutable_intrusive(Args&&... _args);
    template <class T1, class T2>
    friend MutableIntrusivePtr<T1> static_pointer_cast(const MutableIntrusivePtr<T2>& _rp) noexcept;
    template <class T1, class T2>
    friend MutableIntrusivePtr<T1> dynamic_pointer_cast(const MutableIntrusivePtr<T2>& _rp) noexcept;
    template <class T1, class T2>
    friend MutableIntrusivePtr<T1> static_pointer_cast(MutableIntrusivePtr<T2>&& _rp) noexcept;
    template <class T1, class T2>
    friend MutableIntrusivePtr<T1> dynamic_pointer_cast(MutableIntrusivePtr<T2>&& _rp) noexcept;

    MutableIntrusivePtr(T* _ptr)
        : BaseT(_ptr)
    {
    }

    MutableIntrusivePtr(ConstIntrusivePtr<T>&& _other)
        : BaseT(std::move(_other))
    {
    }

    template <class TT>
    MutableIntrusivePtr(ConstIntrusivePtr<TT>&& _other)
        : BaseT(std::move(_other))
    {
    }
};

//-----------------------------------------------------------------------------
//  ConstIntrusivePtr
//-----------------------------------------------------------------------------

template <class T>
class ConstIntrusivePtr : public impl::IntrusivePtrBase<T> {
    using BaseT = impl::IntrusivePtrBase<T>;
    using ThisT = ConstIntrusivePtr<T>;
    template <class TT>
    friend class ConstIntrusivePtr;

public:
    ConstIntrusivePtr() = default;

    ConstIntrusivePtr(T* _ptr, const bool _do_acquire)
        : BaseT(_ptr, _do_acquire)
    {
    }

    ConstIntrusivePtr(MutableIntrusivePtr<T>&& _other)
        : BaseT(std::move(_other))
    {
    }

    template <class TT>
    ConstIntrusivePtr(MutableIntrusivePtr<TT>&& _other)
        : BaseT(std::move(_other))
    {
    }

    ConstIntrusivePtr(ConstIntrusivePtr const& _other)
        : BaseT(_other)
    {
    }

    ConstIntrusivePtr(ConstIntrusivePtr&& _other)
        : BaseT(std::move(_other))
    {
    }

    template <class TT>
    ConstIntrusivePtr(const ConstIntrusivePtr<TT>& _other)
        : BaseT(_other)
    {
    }

    template <class TT>
    ConstIntrusivePtr(ConstIntrusivePtr<TT>&& _other)
        : BaseT(std::move(_other))
    {
    }

    ~ConstIntrusivePtr() = default;

    ConstIntrusivePtr& operator=(const ConstIntrusivePtr& _other) noexcept
    {
        BaseT::doCopy(_other);
        return *this;
    }

    ConstIntrusivePtr& operator=(ConstIntrusivePtr&& _other) noexcept
    {
        BaseT::doMove(std::move(_other));
        return *this;
    }

    template <class TT>
    ConstIntrusivePtr& operator=(const ConstIntrusivePtr<TT>& _other) noexcept
    {
        BaseT::doCopy(_other);
        return *this;
    }

    template <class TT>
    ConstIntrusivePtr& operator=(ConstIntrusivePtr<TT>&& _other) noexcept
    {
        BaseT::doMove(std::move(_other));
        return *this;
    }

    const T* get() const noexcept
    {
        return BaseT::ptr_;
    }

    T const& operator*() const noexcept
    {
        return *BaseT::ptr_;
    }

    const T* operator->() const noexcept
    {
        return BaseT::ptr_;
    }

    MutableIntrusivePtr<T> collapse()
    {
        if (BaseT::ptr_) {
            if (impl::intrusive_ptr_release(*BaseT::ptr_)) {
                impl::intrusive_ptr_acquire(*BaseT::ptr_);
                return MutableIntrusivePtr<T>{std::move(*this)};
            }
            BaseT::ptr_ = nullptr;
        }
        return {};
    }

private:
    template <class T1, class T2>
    friend ConstIntrusivePtr<T1> static_pointer_cast(const ConstIntrusivePtr<T2>& _rp) noexcept;
    template <class T1, class T2>
    friend ConstIntrusivePtr<T1> dynamic_pointer_cast(const ConstIntrusivePtr<T2>& _rp) noexcept;
    template <class T1, class T2>
    friend ConstIntrusivePtr<T1> static_pointer_cast(ConstIntrusivePtr<T2>&& _rp) noexcept;
    template <class T1, class T2>
    friend ConstIntrusivePtr<T1> dynamic_pointer_cast(ConstIntrusivePtr<T2>&& _rp) noexcept;

    ConstIntrusivePtr(T* _ptr)
        : BaseT(_ptr)
    {
    }
};

//-----------------------------------------------------------------------------
// IntrusivePtr
//-----------------------------------------------------------------------------

template <class TT, class... Args>
inline IntrusivePtr<TT> make_intrusive(Args&&... _args)
{
    // return IntrusivePtr<TT>(new TT(std::forward<Args>(_args)...));
    return IntrusivePtr<TT>(impl::intrusive_ptr_create<TT>(std::forward<Args>(_args)...));
}
template <class T1, class T2>
inline IntrusivePtr<T1> static_pointer_cast(const IntrusivePtr<T2>& _rp) noexcept
{
    return IntrusivePtr<T1>(static_cast<T1*>(_rp.get()));
}
template <class T1, class T2>
inline IntrusivePtr<T1> dynamic_pointer_cast(const IntrusivePtr<T2>& _rp) noexcept
{
    return IntrusivePtr<T1>(dynamic_cast<T1*>(_rp.get()));
}

template <class T1, class T2>
inline IntrusivePtr<T1> static_pointer_cast(IntrusivePtr<T2>&& _rp) noexcept
{
    return IntrusivePtr<T1>(static_cast<T1*>(_rp.detach()));
}

template <class T1, class T2>
inline IntrusivePtr<T1> dynamic_pointer_cast(IntrusivePtr<T2>&& _rp) noexcept
{
    auto* pt = dynamic_cast<T1*>(_rp.get());
    if (pt) {
        _rp.detach();
        return IntrusivePtr<T1>(pt);
    }
    return IntrusivePtr<T1>();
}

//-----------------------------------------------------------------------------
// MutableIntrusivePtr
//-----------------------------------------------------------------------------

template <class TT, class... Args>
inline MutableIntrusivePtr<TT> make_mutable_intrusive(Args&&... _args)
{
    // return IntrusivePtr<TT>(new TT(std::forward<Args>(_args)...));
    return MutableIntrusivePtr<TT>(make_intrusive<TT>(std::forward<Args>(_args)...));
}
template <class T1, class T2>
inline MutableIntrusivePtr<T1> static_pointer_cast(const MutableIntrusivePtr<T2>& _rp) noexcept
{
    return MutableIntrusivePtr<T1>(static_cast<T1*>(_rp.get()));
}
template <class T1, class T2>
inline MutableIntrusivePtr<T1> dynamic_pointer_cast(const MutableIntrusivePtr<T2>& _rp) noexcept
{
    return MutableIntrusivePtr<T1>(dynamic_cast<T1*>(_rp.get()));
}

template <class T1, class T2>
inline MutableIntrusivePtr<T1> static_pointer_cast(MutableIntrusivePtr<T2>&& _rp) noexcept
{
    return MutableIntrusivePtr<T1>(static_cast<T1*>(_rp.detach()));
}

template <class T1, class T2>
inline MutableIntrusivePtr<T1> dynamic_pointer_cast(MutableIntrusivePtr<T2>&& _rp) noexcept
{
    auto* pt = dynamic_cast<T1*>(_rp.get());
    if (pt) {
        _rp.detach();
        return MutableIntrusivePtr<T1>(pt);
    }
    return MutableIntrusivePtr<T1>();
}

//-----------------------------------------------------------------------------
// ConstIntrusivePtr
//-----------------------------------------------------------------------------

template <class T1, class T2>
inline ConstIntrusivePtr<T1> static_pointer_cast(const ConstIntrusivePtr<T2>& _rp) noexcept
{
    return ConstIntrusivePtr<T1>(static_cast<T1*>(_rp.get()));
}
template <class T1, class T2>
inline ConstIntrusivePtr<T1> dynamic_pointer_cast(const ConstIntrusivePtr<T2>& _rp) noexcept
{
    return ConstIntrusivePtr<T1>(dynamic_cast<T1*>(_rp.get()));
}

template <class T1, class T2>
inline ConstIntrusivePtr<T1> static_pointer_cast(ConstIntrusivePtr<T2>&& _rp) noexcept
{
    return ConstIntrusivePtr<T1>(static_cast<T1*>(_rp.detach()));
}

template <class T1, class T2>
inline ConstIntrusivePtr<T1> dynamic_pointer_cast(ConstIntrusivePtr<T2>&& _rp) noexcept
{
    auto* pt = dynamic_cast<T1*>(_rp.get());
    if (pt) {
        _rp.detach();
        return ConstIntrusivePtr<T1>(pt);
    }
    return ConstIntrusivePtr<T1>();
}
//-----------------------------------------------------------------------------
} // namespace solid