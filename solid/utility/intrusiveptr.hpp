#pragma once

#include <atomic>
#include <cstddef>

namespace solid {

class IntrusiveThreadSafePolicy;

class IntrusiveThreadSafeBase {
    mutable std::atomic_size_t use_count_{0};

public:
    size_t useCount() const noexcept
    {
        return use_count_.load(std::memory_order_relaxed);
    }

private:
    friend class IntrusiveThreadSafePolicy;
    void acquire() const noexcept
    {
        ++use_count_;
    }
    bool release() const noexcept
    {
        return use_count_.fetch_sub(1) == 1;
    }
};

namespace impl {
template <class T, class P, class... Args>
auto make_policy_intrusive(const P& _policy, Args&&... _args);
}

class IntrusiveThreadSafePolicy {
protected:
    void acquire(const IntrusiveThreadSafeBase& _rt) const noexcept
    {
        _rt.acquire();
    }

    auto release(const IntrusiveThreadSafeBase& _rt) const noexcept
    {
        return _rt.release();
    }
    auto useCount(const IntrusiveThreadSafeBase& _rt) const noexcept
    {
        return _rt.useCount();
    }
};

template <class T, class Policy = IntrusiveThreadSafePolicy>
class IntrusivePtr : public Policy {
    T* ptr_ = nullptr;

public:
    using element_type = T;
    IntrusivePtr()     = default;

    IntrusivePtr(const IntrusivePtr& _other)
        : ptr_(_other.ptr_)
    {
        if (ptr_) {
            Policy::acquire(*ptr_);
        }
    }

    IntrusivePtr(IntrusivePtr&& _other)
        : ptr_(_other.detach())
    {
    }

    template <class What>
    IntrusivePtr(const IntrusivePtr<What, Policy>& _other)
        : ptr_(_other.get())
    {
        if (ptr_) {
            Policy::acquire(*ptr_);
        }
    }

    template <class What>
    IntrusivePtr(IntrusivePtr<What, Policy>&& _other)
        : ptr_(_other.detach())
    {
    }

    template <class U, class P>
    friend class IntrusivePtr;

    ~IntrusivePtr()
    {
        if (ptr_ && Policy::release(*ptr_)) {
            delete ptr_;
        }
    }

    IntrusivePtr& operator=(const IntrusivePtr& _other) noexcept
    {
        IntrusivePtr{_other}.swap(*this);
        return *this;
    }
    IntrusivePtr& operator=(IntrusivePtr&& _other) noexcept
    {
        IntrusivePtr{std::move(_other)}.swap(*this);
        return *this;
    }

    template <class What>
    IntrusivePtr& operator=(const IntrusivePtr<What, Policy>& _other) noexcept
    {
        IntrusivePtr{_other}.swap(*this);
        return *this;
    }
    template <class What>
    IntrusivePtr& operator=(IntrusivePtr<What, Policy>&& _other) noexcept
    {
        IntrusivePtr{std::move(_other)}.swap(*this);
        return *this;
    }

    T* get() const noexcept
    {
        return ptr_;
    }

    T* detach() noexcept
    {
        T* ret = ptr_;
        ptr_   = nullptr;
        return ret;
    }

    T& operator*() const noexcept
    {
        return *ptr_;
    }

    T* operator->() const noexcept
    {
        return ptr_;
    }

    explicit operator bool() const noexcept
    {
        return ptr_ != nullptr;
    }

    void reset()
    {
        if (ptr_) {
            if (Policy::release(*ptr_)) {
                delete ptr_;
            }
            ptr_ = nullptr;
        }
    }

    void swap(IntrusivePtr& _other) noexcept
    {
        T* tmp      = ptr_;
        ptr_        = _other.ptr_;
        _other.ptr_ = tmp;
    }

    size_t useCount() const noexcept
    {
        if (ptr_ != nullptr) [[likely]] {
            return Policy::useCount(*ptr_);
        }
        return 0;
    }

    bool collapse()
    {
        if (ptr_) {
            if (Policy::release(*ptr_)) {
                Policy::acquire(*ptr_);
                return true;
            }
        }
        return false;
    }

private:
    template <class TT, class... Args>
    friend auto make_intrusive(Args&&... _args);
    template <class TT, class PP, class... Args>
    friend auto impl::make_policy_intrusive(const PP&, Args&&... _args);
    template <class T1, class T2, class P>
    friend IntrusivePtr<T1, P> static_pointer_cast(const IntrusivePtr<T2, P>& _rp) noexcept;
    template <class T1, class T2, class P>
    friend IntrusivePtr<T1, P> dynamic_pointer_cast(const IntrusivePtr<T2, P>& _rp) noexcept;
    template <class T1, class T2, class P>
    friend IntrusivePtr<T1, P> static_pointer_cast(IntrusivePtr<T2, P>&& _rp) noexcept;
    template <class T1, class T2, class P>
    friend IntrusivePtr<T1, P> dynamic_pointer_cast(IntrusivePtr<T2, P>&& _rp) noexcept;
    template <class What, class Cache>
    friend class EnableCacheable;

    IntrusivePtr(T* _ptr)
        : ptr_(_ptr)
    {
        if (_ptr) {
            Policy::acquire(*_ptr);
        }
    }

    IntrusivePtr(const Policy& _policy, T* _ptr)
        : Policy(_policy)
        , ptr_(_ptr)
    {
        if (_ptr) {
            Policy::acquire(*_ptr);
        }
    }
};

namespace impl {
template <class T, class P, class... Args>
auto make_policy_intrusive(const P& _policy, Args&&... _args)
{
    return IntrusivePtr<T, P>(_policy, new T(std::forward<Args>(_args)...));
}
} // namespace impl
template <class T, class... Args>
auto make_intrusive(Args&&... _args)
{
    if constexpr (std::is_base_of_v<IntrusiveThreadSafeBase, T>) {
        return IntrusivePtr<T>(new T(std::forward<Args>(_args)...));
    } else {
        return impl::make_policy_intrusive<T>(std::forward<Args>(_args)...);
    }
}

template <class T1, class T2, class P>
IntrusivePtr<T1, P> static_pointer_cast(const IntrusivePtr<T2, P>& _rp) noexcept
{
    return IntrusivePtr<T1, P>(static_cast<T1*>(_rp.get()));
}

template <class T1, class T2, class P>
IntrusivePtr<T1, P> static_pointer_cast(IntrusivePtr<T2, P>&& _rp) noexcept
{
    return IntrusivePtr<T1, P>(static_cast<T1*>(_rp.detach()));
}

template <class T1, class T2, class P>
IntrusivePtr<T1, P> dynamic_pointer_cast(const IntrusivePtr<T2, P>& _rp) noexcept
{
    return IntrusivePtr<T1, P>(dynamic_cast<T1*>(_rp.get()));
}

template <class T1, class T2, class P>
IntrusivePtr<T1, P> dynamic_pointer_cast(IntrusivePtr<T2, P>&& _rp) noexcept
{
    auto* pt = dynamic_cast<T1*>(_rp.get());
    if (pt) {
        _rp.detach();
        return IntrusivePtr<T1, P>(pt);
    }
    return IntrusivePtr<T1, P>();
}

} // namespace solid