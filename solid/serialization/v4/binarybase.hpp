// solid/serialization/v4/binarybase.hpp
//
// Copyright (c) 2026 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <ranges>
#include <type_traits>
#include <utility>

#include "solid/reflection/v2/reflection.hpp"
#include "solid/reflection/v2/typemap.hpp"
#include "solid/serialization/v4/binarybasic.hpp"
#include "solid/serialization/v4/error.hpp"
#include "solid/system/common.hpp"

namespace solid::serialization::inline v4::binary {

enum struct ReturnE : uint8_t {
    Done = 0, // the task finished - it is removed (its children, if any, still run)
    Continue, // run the task again right away
    Wait // run the task again once its children are done - or, if it has none, on the next buffer
};

inline constexpr size_t no_limit = std::numeric_limits<size_t>::max();

namespace detail {

namespace refl = solid::reflection::v2;

// —— TaskQueue ——
// The pending work of a serializer/deserializer once a buffer boundary has
// been hit: an intrusive list of tasks, each a closure resumed with the next
// buffer. It keeps the ordering rules of the v3 run list: while a task runs,
// the tasks it schedules are inserted in front of it (before the "sentinel"),
// so they run - in the order they were scheduled - before the task resumes
// and before its later siblings.
//
// Unlike v3 (std::list nodes + a heap allocated std::function per closure),
// task closures live inline in fixed size blocks recycled through a free list,
// so steady state scheduling does not allocate. Tasks never move once
// scheduled: a closure may own data (e.g. the element of a set being built)
// referenced by the tasks it schedules.
template <class Owner, class Ctx>
class TaskQueue : NonCopyable {
    struct Task {
        Task* prev_                          = nullptr;
        Task* next_                          = nullptr;
        ReturnE (*run_)(Task&, Owner&, Ctx&) = nullptr;
        void (*destroy_)(Task&, TaskQueue&)  = nullptr;
    };

    template <class F>
    struct TaskT : Task {
        F fnc_;

        explicit TaskT(F&& _fnc)
            : fnc_(std::move(_fnc))
        {
        }
    };

    struct FreeBlock {
        FreeBlock* next_;
    };

    static constexpr size_t block_size  = 128;
    static constexpr size_t block_align = alignof(std::max_align_t);

    template <class F>
    static constexpr bool is_pooled_v = sizeof(TaskT<F>) <= block_size && alignof(TaskT<F>) <= block_align;

    Task       head_;
    Task*      sentinel_ = &head_;
    FreeBlock* pfree_    = nullptr;

    template <class F>
    static ReturnE runTask(Task& _rt, Owner& _rowner, Ctx& _rctx)
    {
        return static_cast<TaskT<F>&>(_rt).fnc_(_rowner, _rctx);
    }

    template <class F>
    static void destroyTask(Task& _rt, TaskQueue& _rq)
    {
        auto* pt = static_cast<TaskT<F>*>(&_rt);
        if constexpr (is_pooled_v<F>) {
            pt->~TaskT<F>();
            auto* pb   = reinterpret_cast<FreeBlock*>(pt);
            pb->next_  = _rq.pfree_;
            _rq.pfree_ = pb;
        } else {
            delete pt;
        }
    }

    void* allocateBlock()
    {
        if (pfree_ != nullptr) {
            auto* pb = pfree_;
            pfree_   = pb->next_;
            return pb;
        }
        return ::operator new(block_size, std::align_val_t{block_align});
    }

    void link(Task* _pt) noexcept
    {
        _pt->next_              = sentinel_;
        _pt->prev_              = sentinel_->prev_;
        sentinel_->prev_->next_ = _pt;
        sentinel_->prev_        = _pt;
    }

    static void unlink(Task* _pt) noexcept
    {
        _pt->prev_->next_ = _pt->next_;
        _pt->next_->prev_ = _pt->prev_;
    }

    void erase(Task* _pt)
    {
        unlink(_pt);
        _pt->destroy_(*_pt, *this);
    }

    ReturnE execute(Task* _pt, Owner& _rowner, Ctx& _rctx)
    {
        Task* const pold = sentinel_;
        sentinel_        = _pt;
        const ReturnE rv = _pt->run_(*_pt, _rowner, _rctx);
        sentinel_        = pold;
        return rv;
    }

public:
    TaskQueue()
    {
        head_.prev_ = head_.next_ = &head_;
    }

    ~TaskQueue()
    {
        clear();
        while (pfree_ != nullptr) {
            auto* pb = pfree_;
            pfree_   = pb->next_;
            ::operator delete(pb, block_size, std::align_val_t{block_align});
        }
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return head_.next_ == &head_;
    }

    // True when nothing is pending ahead of the current position: the caller
    // may then work directly on the buffer instead of scheduling.
    [[nodiscard]] bool isRunEmpty() const noexcept
    {
        return sentinel_ == head_.next_;
    }

    template <class F>
    void schedule(F&& _fnc)
    {
        using FncT = std::decay_t<F>;
        TaskT<FncT>* pt;
        if constexpr (is_pooled_v<FncT>) {
            pt = ::new (allocateBlock()) TaskT<FncT>(std::forward<F>(_fnc));
        } else {
            pt = new TaskT<FncT>(std::forward<F>(_fnc));
        }
        pt->run_     = &runTask<FncT>;
        pt->destroy_ = &destroyTask<FncT>;
        link(pt);
    }

    // Schedules the task and, when nothing is pending ahead of it, runs it on
    // the spot (it stays scheduled only if it is not done).
    template <class F>
    void tryRun(F&& _fnc, Owner& _rowner, Ctx& _rctx)
    {
        const bool run_now = isRunEmpty();
        schedule(std::forward<F>(_fnc));
        if (run_now) {
            Task* const pt = head_.next_;
            if (execute(pt, _rowner, _rctx) == ReturnE::Done) {
                erase(pt);
            }
        }
    }

    void run(Owner& _rowner, Ctx& _rctx)
    {
        while (!empty() && !_rowner.error()) {
            Task* const   pt = head_.next_;
            const ReturnE rv = execute(pt, _rowner, _rctx);
            if (rv == ReturnE::Done) {
                erase(pt);
            } else if (rv == ReturnE::Wait && head_.next_ == pt) {
                break; // no children - needs the next buffer
            }
        }
    }

    void clear()
    {
        while (!empty()) {
            erase(head_.next_);
        }
        sentinel_ = &head_;
    }
};

// —— value classification ——
// How a value is put on the wire. Mirrors v3 so that the bytes are identical
// for the types both versions support.
enum struct KindE : uint8_t {
    Bool, // 1 byte: 0xFF / 0xAA
    Integer, // sizeof(T) bytes, little endian
    Enum, // its underlying integer (v4 only)
    String, // compacted size + bytes
    IntegralRange, // contiguous range of non-bool integers: compacted size + raw bytes
    IntegralArray, // std::array of integers (bool included): compacted N + raw bytes
    Array, // other std::array: compacted count + count items
    Bitset, // compacted N + packed bits
    BitVector, // std::vector<bool>: compacted size + packed bits
    Container, // compacted size + items
    KeyContainer, // (multi)set/map: compacted size + items
    Pointer, // typemap: type id + object; no typemap: presence byte + object
    Optional, // presence byte + value (v4 only)
    Variant, // compacted index + alternative (v4 only)
    Reflectable, // its fields, in reflection order
};

template <class T>
struct is_std_pair : std::false_type {};
template <class T1, class T2>
struct is_std_pair<std::pair<T1, T2>> : std::true_type {};

template <class T>
inline constexpr bool is_std_pair_v = is_std_pair<T>::value;

// The item a (multi)set/map is filled from: a map's pair<const K, V> is
// built as a pair<K, V>.
template <class T>
struct key_item {
    using type = T;
};
template <class K, class V>
struct key_item<std::pair<K const, V>> {
    using type = std::pair<K, V>;
};

template <class T>
constexpr bool is_non_bool_integral_v = std::is_integral_v<T> && !std::is_same_v<T, bool>;

template <class T>
consteval KindE kind()
{
    using ValueT = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<ValueT, bool>) {
        return KindE::Bool;
    } else if constexpr (std::is_integral_v<ValueT>) {
        return KindE::Integer;
    } else if constexpr (std::is_enum_v<ValueT>) {
        return KindE::Enum;
    } else if constexpr (refl::detail::StringC<ValueT>) {
        return KindE::String;
    } else if constexpr (refl::detail::StdArrayC<ValueT>) {
        if constexpr (std::is_integral_v<typename ValueT::value_type>) {
            return KindE::IntegralArray;
        } else {
            return KindE::Array;
        }
    } else if constexpr (refl::detail::BitsetC<ValueT>) {
        return KindE::Bitset;
    } else if constexpr (refl::detail::BitVectorC<ValueT>) {
        return KindE::BitVector;
    } else if constexpr (refl::detail::KeyContainerC<ValueT> || refl::detail::KeyValueContainerC<ValueT>) {
        return KindE::KeyContainer;
    } else if constexpr (refl::detail::ContainerC<ValueT>) {
        if constexpr (std::ranges::contiguous_range<ValueT> && is_non_bool_integral_v<typename ValueT::value_type>) {
            return KindE::IntegralRange;
        } else {
            return KindE::Container;
        }
    } else if constexpr (refl::detail::is_pointer_v<ValueT>) {
        return KindE::Pointer;
    } else if constexpr (refl::detail::is_optional_v<ValueT>) {
        return KindE::Optional;
    } else if constexpr (refl::detail::is_variant_v<ValueT>) {
        return KindE::Variant;
    } else {
        return KindE::Reflectable;
    }
}

// The size limit a descriptor carries. String and (key)container
// descriptors default their max_size to 0, meaning "no limit".
template <class Field>
constexpr size_t limit_of(Field const& _rfield) noexcept
{
    using FieldT = std::remove_cvref_t<Field>;
    if constexpr (requires { _rfield.max_size; }) {
        constexpr auto tid = FieldT::type_id;
        if constexpr (tid == refl::TypeIdE::String || tid == refl::TypeIdE::Container || tid == refl::TypeIdE::KeyContainer || tid == refl::TypeIdE::KeyValueContainer) {
            return _rfield.max_size == 0 ? no_limit : static_cast<size_t>(_rfield.max_size);
        } else {
            return static_cast<size_t>(_rfield.max_size);
        }
    } else {
        return no_limit;
    }
}

template <class Field>
struct callable_traits;

template <class V>
struct callable_traits<refl::Callable<V>> {
    using value_type                      = std::decay_t<V>;
    static constexpr bool is_lvalue_bound = std::is_lvalue_reference_v<V>;
};

// The callable behind a Callable descriptor, as an owned value: moved from a
// temporary (the usual make("name", [](auto&, Ctx&){...}) form), copied when
// the descriptor was built over a named callable.
template <class Field>
auto take_callable(Field& _rfield)
{
    using TraitsT = callable_traits<std::remove_cvref_t<Field>>;
    auto& rfnc    = refl::detail::Access::ref(_rfield);
    if constexpr (TraitsT::is_lvalue_bound) {
        return typename TraitsT::value_type(rfnc);
    } else {
        return typename TraitsT::value_type(std::move(rfnc));
    }
}

// Calls the stream progress callback of a stream descriptor if it has one:
// progress(stream, total_bytes, done, ctx).
template <class Progress, class Stream, class Ctx>
void notify_progress(Progress& _rprogress, Stream& _rstream, const uint64_t _total, const bool _done, Ctx& _rctx)
{
    if constexpr (std::is_invocable_v<Progress&, Stream&, uint64_t, bool, Ctx&>) {
        _rprogress(_rstream, _total, _done, _rctx);
    }
}

template <class Field>
auto progress_of(Field const& _rfield)
{
    if constexpr (requires { _rfield.progress; }) {
        return _rfield.progress;
    } else {
        return nullptr;
    }
}

} // namespace detail
} // namespace solid::serialization::inline v4::binary
