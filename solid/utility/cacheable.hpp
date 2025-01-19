// solid/utility/cacheable.hpp
//
// Copyright (c) 2023 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/utility/cast.hpp"
#include "solid/utility/intrusiveptr.hpp"
#include "solid/utility/stack.hpp"

namespace solid {

class Cacheable {
protected:
    virtual void doCache() {}
    virtual void cacheableClear() {}

public:
    virtual ~Cacheable() {}
};

class SharedCacheable : public Cacheable, public std::enable_shared_from_this<SharedCacheable> {

    template <class What, class Cache>
    friend class EnableCacheable;

    std::shared_ptr<SharedCacheable> next_;

    void doAttach(std::shared_ptr<SharedCacheable>&& _other)
    {
        if (_other) {
            _other->next_ = std::move(next_);
            next_         = std::move(_other);
        }
    }

public:
    void cache()
    {
        auto next = std::move(next_);
        cacheableClear();
        doCache();
        while (next) {
            auto tmp = std::move(next->next_);
            next->cacheableClear();
            next->doCache();
            std::swap(next, tmp);
        }
    }

    template <class Other>
    void cacheableAttach(const std::shared_ptr<Other>& _other)
    {
        doAttach(std::static_pointer_cast<SharedCacheable>(_other));
    }

    template <class Other>
    void cacheableAttach(std::shared_ptr<Other>&& _other)
    {
        doAttach(std::static_pointer_cast<SharedCacheable>(std::move(_other)));
    }
};

class IntrusiveCacheable : public Cacheable, public IntrusiveThreadSafeBase {

    template <class What, class Cache>
    friend class EnableCacheable;

    IntrusivePtr<IntrusiveCacheable> next_;

    void doAttach(IntrusivePtr<IntrusiveCacheable>&& _other)
    {
        if (_other) {
            _other->next_ = std::move(next_);
            next_         = std::move(_other);
        }
    }

public:
    void cache()
    {
        auto next = std::move(next_);
        cacheableClear();
        doCache();
        while (next) {
            auto tmp = std::move(next->next_);
            next->cacheableClear();
            next->doCache();
            std::swap(next, tmp);
        }
    }

    template <class Other>
    void cacheableAttach(const IntrusivePtr<Other>& _other)
    {
        doAttach(static_pointer_cast<IntrusiveCacheable>(_other));
    }

    template <class Other>
    void cacheableAttach(IntrusivePtr<Other>&& _other)
    {
        doAttach(static_pointer_cast<IntrusiveCacheable>(std::move(_other)));
    }
};

#ifdef __cpp_concepts
template <typename What>
concept SharedCacheableDerived = std::is_base_of_v<SharedCacheable, What>;

template <SharedCacheableDerived What>
#else
template <class What>
#endif
void cacheable_cache(std::shared_ptr<What>&& _what_ptr)
{
    if (_what_ptr) [[likely]] {
        _what_ptr->cache();
        _what_ptr.reset();
    }
}

#ifdef __cpp_concepts
template <typename What>
concept IntrusiveCacheableDerived = std::is_base_of_v<IntrusiveCacheable, What>;

template <IntrusiveCacheableDerived What>
#else
template <class What>
#endif
void cacheable_cache(IntrusivePtr<What>&& _what_ptr)
{
    if (_what_ptr) [[likely]] {
        _what_ptr->cache();
        _what_ptr.reset();
    }
}

class ThreadLocalCache;

template <class What, class Cache = ThreadLocalCache>
class EnableCacheable : public What {
    using ThisT = EnableCacheable<What, Cache>;

public:
    static auto create()
    {
        if constexpr (std::is_base_of_v<IntrusiveCacheable, What>) {
            auto ret = static_pointer_cast<ThisT>(Cache::template load_ip<What>());
            if (!ret) [[unlikely]] {
                ret = make_intrusive<ThisT>();
            }
            return ret;
        } else {
            static_assert(std::is_base_of_v<SharedCacheable, What>, "Type must be derived from SharedCacheable");
            auto p{Cache::template load_sp<What>()};
            auto ret = std::static_pointer_cast<ThisT>(std::move(p));
            if (!ret) [[unlikely]] {
                ret = std::make_shared<ThisT>();
            }
            return ret;
        }
    }

    template <typename... Args>
    EnableCacheable(Args&&... _args)
        : What(std::forward<Args>(_args)...)
    {
    }

private:
    void doCache() override
    {
        if constexpr (std::is_base_of_v<IntrusiveCacheable, What>) {
            Cache::store(IntrusivePtr<What>(static_cast<What*>(this)));
        } else {
            static_assert(std::is_base_of_v<SharedCacheable, What>, "Type must be derived from SharedCacheable");
            Cache::store(std::static_pointer_cast<What>(this->shared_from_this()));
        }
    }
};

class ThreadLocalCache {
    template <class What>
    static auto& local_stack_sp()
    {
        static thread_local Stack<std::shared_ptr<What>> stack;
        return stack;
    }

    template <class What>
    static auto& local_stack_ip()
    {
        static thread_local Stack<IntrusivePtr<What>> stack;
        return stack;
    }

public:
    template <class What>
    static void store(std::shared_ptr<What>&& _what)
    {
        local_stack_sp<What>().push(std::move(_what));
    }

    template <class What>
    static void store(IntrusivePtr<What>&& _what)
    {
        local_stack_ip<What>().push(std::move(_what));
    }

    template <class What>
    static auto load_sp()
    {
        auto& ls = local_stack_sp<What>();
        if (!ls.empty()) {
            auto ret = std::move(ls.top());
            ls.pop();
            return ret;
        } else {
            return std::shared_ptr<What>{};
        }
    }

    template <class What>
    static auto load_ip()
    {
        auto& ls = local_stack_ip<What>();
        if (!ls.empty()) {
            auto ret = std::move(ls.top());
            ls.pop();
            return ret;
        } else {
            return IntrusivePtr<What>{};
        }
    }
};

} // namespace solid
