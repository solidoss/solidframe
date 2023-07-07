// solid/utility/cacheable.hpp
//
// Copyright (c) 2023 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/utility/stack.hpp"
#include <memory>

namespace solid {

class Cacheable : public std::enable_shared_from_this<Cacheable> {

    template <class What, class Cache>
    friend class EnableCacheable;

    std::shared_ptr<Cacheable> next_;

    void doAttach(std::shared_ptr<Cacheable>&& _other)
    {
        if (_other) {
            _other->next_ = std::move(next_);
            next_         = std::move(_other);
        }
    }
    virtual void doCache() {}

public:
    void cache()
    {
        auto next = std::move(next_);
        doCache();
        while (next) {
            auto tmp = std::move(next->next_);
            next->doCache();
            std::swap(next, tmp);
        }
    }
    template <class Other>
    void cacheAttach(const std::shared_ptr<Other>& _other)
    {
        doAttach(static_pointer_cast<Cacheable>(_other));
    }
    template <class Other>
    void cacheAttach(std::shared_ptr<Other>&& _other)
    {
        doAttach(static_pointer_cast<Cacheable>(std::move(_other)));
    }
};

template <typename What>
concept CacheableDerived = std::is_base_of_v<Cacheable, What>;

template <CacheableDerived What>
void cacheable_cache(std::shared_ptr<What>&& _what_ptr)
{
    if (_what_ptr) {
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
        std::shared_ptr<ThisT> ret = std::static_pointer_cast<ThisT>(Cache::template load<What>());
        if (!ret) [[unlikely]] {
            ret = std::make_shared<ThisT>();
        }
        return ret;
    }

    template <typename... Args>
    EnableCacheable(Args&&... _args)
        : What(std::forward<Args>(_args)...)
    {
    }

private:
    void doCache() override
    {
        auto what = std::static_pointer_cast<What>(this->shared_from_this());
        Cache::store(std::move(what));
    }
};

class ThreadLocalCache {
    template <class What>
    static Stack<std::shared_ptr<What>>& local_stack()
    {
        static thread_local Stack<std::shared_ptr<What>> stack;
        return stack;
    }

public:
    template <class What>
    static void store(std::shared_ptr<What>&& _what)
    {
        local_stack<What>().push(std::move(_what));
    }

    template <class What>
    static auto load()
    {
        auto& ls = local_stack<What>();
        if (ls.empty()) {
            return std::shared_ptr<What>{};
        } else {
            auto ret = std::move(ls.top());
            ls.pop();
            return ret;
        }
    }
};

} // namespace solid