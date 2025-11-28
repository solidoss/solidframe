// solid/utility/pool.hpp
//
// Copyright (c) 2025 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#pragma once
#include "solid/system/spinlock.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/intrusiveptr.hpp"
#include "solid/utility/poolable.hpp"
#include <atomic>
#include <cassert>
#include <deque>
#include <memory>
#include <type_traits>

namespace solid {

struct PoolConfig {
    size_t push_capacity_ = 1024;
    size_t poll_capacity_ = 0;
};

namespace impl {
constexpr PoolConfig default_pool_config;

template <typename ObjectType, template <typename> typename Ptr>
class PoolBase {
protected:
    static_assert(is_poolable_v<ObjectType>, "ObjectType must be Poolable");
    using ObjectPtrT = Ptr<ObjectType>;

    struct LockFreeEntry {
        std::atomic_flag flag_;
        ObjectPtrT       ptr_;
    };
    struct LockFreeData {
        std::unique_ptr<LockFreeEntry[]> entries_;
        size_t                           pop_index_                                           = 0;
        alignas(solid::hardware_destructive_interference_size) std::atomic_size_t push_index_ = 0;
    };

    PoolConfig const config_;
    LockFreeData     lock_free_data_;

    size_t             create_count_       = 0u;
    std::atomic_size_t await_return_count_ = 0u;
    std::atomic_flag   is_stopping_{};
    std::atomic_flag   can_destroy_{};

    PoolBase(PoolConfig const& _config)
        : config_(_config)
    {
        lock_free_data_.entries_.reset(new LockFreeEntry[config_.push_capacity_]);
    }

    ~PoolBase()
    {
        is_stopping_.test_and_set();
        while (not isFull()) {
            can_destroy_.wait(false);
        }
        for (size_t i = 0; i < config_.push_capacity_; ++i) {
            auto& entry = lock_free_data_.entries_[i];
            if (entry.ptr_) {
                entry.ptr_->ppool_ = nullptr;
            }
        }
    }

    bool isFull()
    {
        return await_return_count_ == 0u;
    }

    [[nodiscard]] ObjectPtrT popLockFree()
    {
        ObjectPtrT ptr;
        auto const index  = lock_free_data_.pop_index_;
        auto&      rentry = lock_free_data_.entries_[index % config_.push_capacity_];

        if (rentry.flag_.test(std::memory_order_acquire)) {
            assert(rentry.ptr_);
            ptr = std::move(rentry.ptr_);
            assert(not rentry.ptr_);
            rentry.flag_.clear(std::memory_order_release);
            ++lock_free_data_.pop_index_;
        }
        return ptr;
    }

    void push(ObjectPtrT&& _msg_ptr)
    {
        if (not _msg_ptr) {
            return;
        }

        {
            auto const index  = lock_free_data_.push_index_.fetch_add(1, std::memory_order_relaxed);
            auto&      rentry = lock_free_data_.entries_[index % config_.push_capacity_];

            while (rentry.flag_.test(std::memory_order_acquire)) {
                cpu_pause();
            }
            assert(not rentry.ptr_);
            rentry.ptr_ = std::move(_msg_ptr);
            rentry.flag_.test_and_set(std::memory_order_release);
        }
        if (await_return_count_.fetch_sub(1, std::memory_order_relaxed) == 1u and is_stopping_.test()) {
            can_destroy_.notify_one();
        }
    }
};

} // namespace impl

template <typename ObjectType>
class Pool<ObjectType, MutableIntrusivePtr> final : protected impl::PoolBase<ObjectType, MutableIntrusivePtr> {
    using BaseT = impl::PoolBase<ObjectType, MutableIntrusivePtr>;
    [[nodiscard]] BaseT::ObjectPtrT pop()
    {
        return this->popLockFree();
    }

    Pool(PoolConfig const& _config)
        : BaseT(_config)
    {
    }

public:
    using ThisT = Pool<ObjectType, MutableIntrusivePtr>;
    static ThisT& get(PoolConfig const& _config = impl::default_pool_config)
    {
        static thread_local ThisT ins{_config};
        return ins;
    }
    template <typename... Args>
    [[nodiscard]] static BaseT::ObjectPtrT create(Args&&... args)
    {
        auto& rthis = get();
        auto  ptr   = rthis.pop();
        if (ptr) {
            ptr->init(std::forward<Args>(args)...);
            if (ptr->ppool_) {
                ++rthis.await_return_count_;
            }
        } else {
            ptr = make_mutable_intrusive<ObjectType>(std::forward<Args>(args)...);
            if (rthis.create_count_ < rthis.config_.push_capacity_) {
                ++rthis.create_count_;
                ptr->ppool_ = std::addressof(rthis);
                ++rthis.await_return_count_;
            }
        }
        return ptr;
    }

    void push(BaseT::ObjectPtrT&& _msg_ptr)
    {
        BaseT::push(std::move(_msg_ptr));
    }
};

template <typename ObjectType>
class Pool<ObjectType, IntrusivePtr> final : protected impl::PoolBase<ObjectType, IntrusivePtr> {
    static_assert(is_poolable_v<ObjectType>, "ObjectType must be Poolable");
    using BaseT      = impl::PoolBase<ObjectType, IntrusivePtr>;
    using ObjectPtrT = BaseT::ObjectPtrT;
    using DequeT     = std::deque<ObjectPtrT>;

    DequeT poll_dq_;

    [[nodiscard]] ObjectPtrT pop()
    {
        if (poll_dq_.empty()) {
            return this->popLockFree();
        }
        ObjectPtrT ptr;
        for (auto it = poll_dq_.begin(); it != poll_dq_.end(); ++it) {
            if (it->useCount() == 1U) {
                ptr = std::move(*it);
                if (it == poll_dq_.begin()) {
                } else {
                    *it = std::move(*poll_dq_.begin());
                }
                poll_dq_.pop_front();
                poll_dq_.emplace_back(ptr);
                break;
            }
            ptr = this->popLockFree();
            if (ptr) {
                break;
            }
        }
        return ptr;
    }

    Pool(PoolConfig const& _config)
        : BaseT(_config)
    {
    }

public:
    using ThisT = Pool<ObjectType, IntrusivePtr>;
    static ThisT& get(PoolConfig const& _config = impl::default_pool_config)
    {
        static thread_local ThisT ins{_config};
        return ins;
    }
    template <typename... Args>
    [[nodiscard]] static ObjectPtrT create(Args&&... args)
    {
        auto& rthis = get();

        auto ptr = rthis.pop();
        if (ptr) {
            ptr->init(std::forward<Args>(args)...);
            if (ptr->ppool_) {
                ++rthis.await_return_count_;
            }
        } else {
            ptr = make_intrusive<ObjectType>(std::forward<Args>(args)...);
            if (rthis.create_count_ < rthis.config_.poll_capacity_) {
                ++rthis.create_count_;
                rthis.poll_dq_.emplace_back(ptr);
            } else if ((rthis.create_count_ - rthis.config_.poll_capacity_) < rthis.config_.push_capacity_) {
                ++rthis.create_count_;
                ptr->ppool_ = std::addressof(rthis);
                ++rthis.await_return_count_;
            }
        }
        return ptr;
    }

    void push(BaseT::ObjectPtrT&& _msg_ptr)
    {
        BaseT::push(std::move(_msg_ptr));
    }
};

template <class T>
template <template <typename> typename Ptr>
void impl::IntrusivePtrBase<T>::doPushToPool(Pool<T, Ptr>* _ppool)
{
    _ppool->push(Ptr<T>(std::move(*this)));
}

} // namespace solid