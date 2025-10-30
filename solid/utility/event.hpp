// solid/event.hpp
//
// Copyright (c) 2014, 2023 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once
#include <cstddef>
#define SOLID_THROW_ON_BIG_EVENT

#include <ostream>
#include <typeindex>
#include <typeinfo>
#include <utility>

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include "solid/utility/any.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/function.hpp"

namespace solid {

class EventCategoryBase;
class EventHandlerBase;

//-----------------------------------------------------------------------------
//      EventBase
//-----------------------------------------------------------------------------

class EventBase {
    friend class EventCategoryBase;
    friend class EventHandlerBase;

    const EventCategoryBase* pcategory_;
    uintptr_t                id_;
    uintptr_t                rtti_  = 0u;
    void*                    pdata_ = nullptr;

public:
    std::ostream& print(std::ostream& _ros) const;

    void reset();

    void resetData()
    {
        auto const rtti = rtti_;
        switch (any_impl::representation(rtti)) {
        [[likely]] case any_impl::RepresentationE::Small: {
            auto& rrtti = any_impl::SmallRTTI::get(rtti);
            if (rrtti.pdestroy_fnc_) {
                rrtti.pdestroy_fnc_(pdata_);
            }
        } break;
        case any_impl::RepresentationE::Big: {
            any_impl::BigRTTI::get(rtti).destroy_fnc_(pdata_);
        } break;
        case any_impl::RepresentationE::None:
        default:
            break;
        }
        rtti_  = 0u;
        pdata_ = nullptr;
    }

    bool hasData() const noexcept
    {
        return rtti_ == 0u;
    }

    bool operator==(const EventBase& _rother) const;

    bool empty() const;

    explicit operator bool() const noexcept
    {
        return !empty();
    }

    template <class T>
    const T* cast() const noexcept
    {
        const std::type_info* const pinfo = typeInfo();
        if (!pinfo || std::type_index(*pinfo) != std::type_index(typeid(T))) {
            return nullptr;
        }

        return static_cast<const T*>(pdata_);
    }

    template <class T>
    T* cast() noexcept
    {
        return const_cast<T*>(static_cast<const EventBase*>(this)->cast<T>());
    }

    template <class T>
    const T* get_if() const noexcept
    {
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return static_cast<const T*>(any_impl::BaseRTTI::get(rtti).get_if_fnc_(std::type_index(typeid(T)), pdata_));
        }
        return nullptr;
    }

    template <class T>
    T* get_if() noexcept
    {
        return const_cast<T*>(static_cast<const EventBase*>(this)->get_if<T>());
    }

    bool is_movable() const
    {
#if 0
        auto const lrtti = rtti_;
        switch (any_impl::representation(lrtti)) {
        case any_impl::RepresentationE::Small:
            return any_impl::SmallRTTI::get(lrtti).is_movable_;
        case any_impl::RepresentationE::Big:
            return any_impl::BigRTTI::get(lrtti).is_movable_;
        default:
            return true;
        }
#endif
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return any_impl::BaseRTTI::get(rtti).is_movable_;
        }
        return true;
    }
    bool is_copyable() const
    {
#if 0
        auto const lrtti = rtti_;
        switch (any_impl::representation(lrtti)) {
        case any_impl::RepresentationE::Small:
            return any_impl::SmallRTTI::get(lrtti).is_copyable_;
        case any_impl::RepresentationE::Big:
            return any_impl::BigRTTI::get(lrtti).is_copyable_;
        default:
            return true;
        }
#endif
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return any_impl::BaseRTTI::get(rtti).is_copyable_;
        }
        return true;
    }

    bool is_tuple() const
    {
#if 0
        auto const lrtti = rtti_;
        switch (any_impl::representation(lrtti)) {
        case any_impl::RepresentationE::Small:
            return any_impl::SmallRTTI::get(lrtti).is_tuple_;
        case any_impl::RepresentationE::Big:
            return any_impl::BigRTTI::get(lrtti).is_tuple_;
        default:
            return false;
        }
#endif
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return any_impl::BaseRTTI::get(rtti).is_tuple_;
        }
        return false;
    }

    bool is_small() const
    {
        return any_impl::representation(rtti_) == any_impl::RepresentationE::Small;
    }
    bool is_big() const
    {
        return any_impl::representation(rtti_) == any_impl::RepresentationE::Big;
    }

protected:
    EventBase(
        const EventCategoryBase& _rcategory,
        const uintptr_t          _id)
        : pcategory_(&_rcategory)
        , id_(_id)

    {
    }

    EventBase(const EventBase& _other)
        : pcategory_(_other.pcategory_)
        , id_(_other.id_)
    {
        rtti_ = _other.rtti_;
    }

    ~EventBase()
    {
        reset();
    }

    const std::type_info* typeInfo() const noexcept
    {
        // return reinterpret_cast<const std::type_info*>(type_data_ & ~any_impl::representation_and_flags_mask);
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return any_impl::BaseRTTI::get(rtti).get_type_info_fnc_();
        }
        return nullptr;
    }

    const std::type_info& type() const noexcept
    {
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return *any_impl::BaseRTTI::get(rtti).get_type_info_fnc_();
        }
        return typeid(void);
    }

    void reset(const EventBase& _other)
    {
        pcategory_ = _other.pcategory_;
        id_        = _other.id_;
        resetData();
    }

    void doMoveFrom(void* _psmall_data, const size_t _small_capacity, const size_t _small_align, EventBase& _other)
    {
        pdata_ = _psmall_data;

        auto const rtti = _other.rtti_;

        if (rtti) [[likely]] {
            rtti_ = any_impl::BaseRTTI::get(rtti).move_fnc_(
                _other.pdata_,
                _psmall_data, _small_capacity, _small_align,
                pdata_);

            if (is_big()) {
                _other.rtti_ = 0u;
            }
            _other.reset();
        }
#if 0
        representation(any_impl::RepresentationE::None);
        switch (_other.representation()) {
        case any_impl::RepresentationE::Small: {
            const auto repr = _other.rtti_.psmall_->pmove_fnc_(
                _other.pdata_,
                _psmall_data, _small_capacity, rtti_.psmall_,
                pdata_, rtti_.pbig_);
            representation(repr);
            _other.reset();
        } break;
        case any_impl::RepresentationE::Big: {
            const auto repr = _other.rtti_.pbig_->pmove_fnc_(
                _other.pdata_,
                _psmall_data, _small_capacity, rtti_.psmall_,
                pdata_, rtti_.pbig_);
            representation(repr);
            if (repr == any_impl::RepresentationE::Big) {
                _other.type_data_ = 0;
            } else {
                _other.reset();
            }
        } break;
        default:
            break;
        }
#endif
    }

    void doCopyFrom(void* _psmall_data, const size_t _small_capacity, const size_t _small_align, const EventBase& _other)
    {
        pdata_ = _psmall_data;

        auto const rtti = _other.rtti_;

        if (rtti) [[likely]] {
            rtti_ = any_impl::BaseRTTI::get(rtti).copy_fnc_(
                _other.pdata_,
                _psmall_data, _small_capacity, _small_align,
                pdata_);
        }
#if 0
        type_data_ = _other.type_data_;
        pdata_     = _psmall_data;
        representation(any_impl::RepresentationE::None);
        switch (_other.representation()) {
        case any_impl::RepresentationE::Small: {
            const auto repr = _other.rtti_.psmall_->pcopy_fnc_(
                _other.pdata_,
                _psmall_data, _small_capacity, rtti_.psmall_,
                pdata_, rtti_.pbig_);
            representation(repr);
        } break;
        case any_impl::RepresentationE::Big: {
            const auto repr = _other.rtti_.pbig_->pcopy_fnc_(
                _other.pdata_,
                _psmall_data, _small_capacity, rtti_.psmall_,
                pdata_, rtti_.pbig_);
            representation(repr);
        } break;
        default:
            break;
        }
#endif
    }

    template <class T, class... Args>
    T& doEmplaceSmall(void* _psmall_data, Args&&... _args)
    {
        auto* pdata = ::new (_psmall_data) T{std::forward<Args>(_args)...};
        pdata_      = _psmall_data;
        rtti_       = any_impl::representation(&any_impl::small_rtti<T>, any_impl::RepresentationE::Small);

        // rtti_.psmall_ = &any_impl::small_rtti<T>;
        // type_data_    = reinterpret_cast<uintptr_t>(&typeid(T));
        // representation(any_impl::RepresentationE::Small);

        return *pdata;
    }

    template <class T, class... Args>
    T& doEmplaceBig(Args&&... _args)
    {
#if defined(SOLID_THROW_ON_BIG_EVENT)
        solid_throw("Big Event");
#endif
        T* const ptr = ::new T(std::forward<Args>(_args)...);
        pdata_       = ptr;
        rtti_        = representation(&any_impl::big_rtti<T>, any_impl::RepresentationE::Big);
        // rtti_.pbig_  = &any_impl::big_rtti<T>;
        // type_data_   = reinterpret_cast<uintptr_t>(&typeid(T));
        // representation(any_impl::RepresentationE::Big);
        return *ptr;
    }
};

std::ostream& operator<<(std::ostream& _ros, EventBase const& _re);

//-----------------------------------------------------------------------------
//      EventCategoryBase
//-----------------------------------------------------------------------------

class EventCategoryBase {
public:
    const std::string& name() const
    {
        return name_;
    }

protected:
    EventCategoryBase(const std::string_view& _name)
        : name_(_name)
    {
    }

    virtual ~EventCategoryBase() {}

    uintptr_t eventId(const EventBase& _revt) const
    {
        return _revt.id_;
    }

private:
    friend class EventBase;
    virtual std::string_view eventName(const EventBase& _revt) const = 0;

private:
    const std::string name_;
};

//-----------------------------------------------------------------------------
//      EventCategory
//-----------------------------------------------------------------------------

template <typename EventIds>
class EventCategory : public EventCategoryBase {
    using FunctionT = std::function<std::string_view(const EventIds)>;

public:
    template <typename F>
    EventCategory(const std::string_view& _name, F _f)
        : EventCategoryBase(_name)
        , names_fnc_(std::move(_f))
    {
    }

private:
    std::string_view eventName(const EventBase& _revt) const override
    {
        return names_fnc_(static_cast<EventIds>(eventId(_revt)));
    }

private:
    FunctionT names_fnc_;
};

enum class GenericEventE : uintptr_t {
    Default,
    Start,
    Stop,
    Wake,
    Message,
    Timer,
    Pause,
    Resume,
    Update,
    Kill,
};

template <typename EventIds>
inline EventCategory<EventIds> category;

template <>
inline EventCategory<GenericEventE> category<GenericEventE>{
    "solid::generic_events",
    [](const GenericEventE _evt) {
        switch (_evt) {
        case GenericEventE::Default:
            return "default";
        case GenericEventE::Start:
            return "start";
        case GenericEventE::Stop:
            return "stop";
        case GenericEventE::Wake:
            return "raise";
        case GenericEventE::Message:
            return "message";
        case GenericEventE::Timer:
            return "timer";
        case GenericEventE::Pause:
            return "pause";
        case GenericEventE::Resume:
            return "resume";
        case GenericEventE::Update:
            return "update";
        case GenericEventE::Kill:
            return "kill";
        default:
            return "unknown";
        }
    }};

//-----------------------------------------------------------------------------
//      Event<>
//-----------------------------------------------------------------------------
template <size_t SmallSize = 0, size_t SmallAlign = SmallSize == 0 ? 0 : sizeof(uintptr_t)>
    requires(SmallSize == 0 and SmallAlign == 0 or (SmallSize > 0 and SmallSize >= SmallAlign and (SmallSize % SmallAlign == 0) and std::popcount(SmallAlign) == 1))
class Event;

template <class T>
struct is_event;

template <size_t V, size_t A>
struct is_event<Event<V, A>> : std::true_type {
};

template <>
struct is_event<EventBase> : std::true_type {
};

template <class T>
struct is_event : std::false_type {
};

template <>
class Event<0, 0> : public EventBase {
public:
    using ThisT = Event<0>;

    static constexpr size_t smallCapacity()
    {
        return 0;
    }

    static constexpr size_t smallAlign()
    {
        return 0;
    }

    template <class T>
    static constexpr bool is_small_type()
    {
        return false;
    }

    Event()
        : EventBase(category<GenericEventE>, to_underlying(GenericEventE::Default))
    {
    }

    template <class Events>
    Event(const Events _event)
        : EventBase(category<Events>, to_underlying(_event))
    {
    }

    template <class Events>
    Event(const EventCategoryBase& _category, const Events _event)
        : EventBase(_category, to_underlying(_event))
    {
    }
};

#if 0 // TODO:remove
namespace event_impl {
constexpr size_t compute_small_capacity(const size_t _req_capacity)
{
    static constexpr size_t default_total_size = 32u;

    const size_t end_capacity = sizeof(uintptr_t) + sizeof(void*);
    const size_t req_capacity = std::max(_req_capacity, std::max(end_capacity, sizeof(max_align_t)) - end_capacity);
    const size_t tot_capacity = std::max(default_total_size, padded_size(req_capacity + sizeof(uintptr_t) + sizeof(void*), alignof(max_align_t)));

    return tot_capacity - end_capacity;
}
} // namespace event_impl
#endif

template <size_t SmallSize, size_t SmallAlign>
    requires(SmallSize == 0 and SmallAlign == 0 or (SmallSize > 0 and SmallSize >= SmallAlign and (SmallSize % SmallAlign == 0) and std::popcount(SmallAlign) == 1))
class Event : public EventBase {
    alignas(SmallAlign) unsigned char data_[SmallSize];

public:
    using ThisT = Event<SmallSize, SmallAlign>;

    static constexpr size_t smallCapacity()
    {
        return SmallSize;
    }

    static constexpr size_t smallAlign()
    {
        return SmallAlign;
    }

    template <class T>
    static constexpr bool is_small_type()
    {
        return alignof(T) <= smallAlign() && sizeof(T) <= smallCapacity();
    }

    Event()
        : EventBase(category<GenericEventE>, to_underlying(GenericEventE::Default))
    {
    }

    Event(const ThisT& _other)
        : EventBase(_other)
    {
        doCopyFrom(data_, smallCapacity(), smallAlign(), _other);
    }

    template <size_t Sz>
    Event(const Event<Sz>& _other)
        : EventBase(_other)
    {
        doCopyFrom(data_, smallCapacity(), smallAlign(), _other);
    }

    Event(ThisT&& _other)
        : EventBase(_other)
    {
        doMoveFrom(data_, smallCapacity(), smallAlign(), _other);
    }

    template <size_t Sz, size_t Al>
    Event(Event<Sz, Al>&& _other)
        : EventBase(_other)
    {
        doMoveFrom(data_, smallCapacity(), smallAlign(), _other);
    }

    template <typename Evs, class T, std::enable_if_t<std::conjunction_v<std::negation<is_event<std::decay_t<T>>>, std::negation<is_specialization<std::decay_t<T>, std::in_place_type_t>>>, int> = 0>
    Event(const Evs _ev, T&& _rvalue)
        : EventBase(category<Evs>, to_underlying(_ev))
    {

        if constexpr (is_small_type<T>()) {
            auto& rval = reinterpret_cast<T&>(data_);
            doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), std::forward<T>(_rvalue));
        } else {
            doEmplaceBig<std::decay_t<T>>(std::forward<T>(_rvalue));
        }
    }

    template <typename Evs, class T, std::enable_if_t<std::conjunction_v<std::negation<is_event<std::decay_t<T>>>, std::negation<is_specialization<std::decay_t<T>, std::in_place_type_t>>>, int> = 0>
    Event(const EventCategoryBase& _category, const Evs _ev, T&& _rvalue)
        : EventBase(_category, to_underlying(_ev))
    {

        if constexpr (is_small_type<T>()) {
            auto& rval = reinterpret_cast<T&>(data_);
            doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), std::forward<T>(_rvalue));
        } else {
            doEmplaceBig<std::decay_t<T>>(std::forward<T>(_rvalue));
        }
    }

    template <typename Evs, class T, class... Args,
        std::enable_if_t<
            std::conjunction_v<std::is_constructible<std::decay_t<T>, Args...>>,
            int>
        = 0>
    explicit Event(const Evs _ev, std::in_place_type_t<T>, Args&&... _args)
        : EventBase(category<Evs>, to_underlying(_ev))
    {
        if constexpr (is_small_type<T>()) {
            auto& rval = reinterpret_cast<T&>(data_);
            doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), std::forward<Args>(_args)...);
        } else {
            doEmplaceBig<std::decay_t<T>>(std::forward<Args>(_args)...);
        }
    }

    template <typename Evs, class T, class... Args,
        std::enable_if_t<
            std::conjunction_v<std::is_constructible<std::decay_t<T>, Args...>>,
            int>
        = 0>
    explicit Event(const EventCategoryBase& _category, const Evs _ev, std::in_place_type_t<T>, Args&&... _args)
        : EventBase(_category, to_underlying(_ev))
    {
        if constexpr (is_small_type<T>()) {
            auto& rval = reinterpret_cast<T&>(data_);
            doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), std::forward<Args>(_args)...);
        } else {
            doEmplaceBig<std::decay_t<T>>(std::forward<Args>(_args)...);
        }
    }

    template <typename Evs, class T, class E, class... Args,
        std::enable_if_t<std::conjunction_v<std::is_constructible<std::decay_t<T>, std::initializer_list<E>&, Args...>,
                             std::is_copy_constructible<std::decay_t<T>>>,
            int>
        = 0>
    explicit Event(const Evs _ev, std::in_place_type_t<T>, std::initializer_list<E> _ilist, Args&&... _args)
        : EventBase(category<Evs>, to_underlying(_ev))
    {
        if constexpr (is_small_type<T>()) {
            auto& rval = reinterpret_cast<T&>(data_);
            doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), _ilist, std::forward<Args>(_args)...);
        } else {
            doEmplaceBig<std::decay_t<T>>(_ilist, std::forward<Args>(_args)...);
        }
    }

    template <typename Evs, class T, class E, class... Args,
        std::enable_if_t<std::conjunction_v<std::is_constructible<std::decay_t<T>, std::initializer_list<E>&, Args...>,
                             std::is_copy_constructible<std::decay_t<T>>>,
            int>
        = 0>
    explicit Event(const EventCategoryBase& _category, const Evs _ev, std::in_place_type_t<T>, std::initializer_list<E> _ilist, Args&&... _args)
        : EventBase(_category, to_underlying(_ev))
    {
        if constexpr (is_small_type<T>()) {
            auto& rval = reinterpret_cast<T&>(data_);
            doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), _ilist, std::forward<Args>(_args)...);
        } else {
            doEmplaceBig<std::decay_t<T>>(_ilist, std::forward<Args>(_args)...);
        }
    }

    Event(const EventBase& _other)
        : EventBase(_other)
    {
        doCopyFrom(data_, smallCapacity(), smallAlign(), _other);
    }

    Event(EventBase&& _other) noexcept
        : EventBase(_other)
    {
        doMoveFrom(data_, smallCapacity(), smallAlign(), _other);
    }

    ThisT& operator=(const ThisT& _other)
    {
        *this = ThisT{_other};
        return *this;
    }

    ThisT& operator=(ThisT&& _other) noexcept
    {
        reset(_other);
        doMoveFrom(data_, smallCapacity(), smallAlign(), _other);
        return *this;
    }

    template <size_t Sz, size_t Al>
    ThisT& operator=(const Event<Sz, Al>& _other)
    {
        *this = ThisT{_other};
        return *this;
    }

    template <size_t Sz, size_t Al>
    ThisT& operator=(Event<Sz, Al>&& _other) noexcept
    {
        reset(_other);
        doMoveFrom(data_, smallCapacity(), smallAlign(), _other);
        return *this;
    }

    template <class T, std::enable_if_t<std::conjunction_v<std::negation<is_event<std::decay_t<T>>>, std::is_copy_constructible<std::decay_t<T>>>, int> = 0>
    ThisT& operator=(T&& _rvalue)
    {
        resetData();

        if constexpr (is_small_type<T>()) {
            auto& rval = reinterpret_cast<T&>(data_);
            doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), std::forward<T>(_rvalue));
        } else {
            doEmplaceBig<std::decay_t<T>>(std::forward<T>(_rvalue));
        }
        return *this;
    }

    template <class T, class... Args,
        std::enable_if_t<
            std::conjunction_v<std::is_constructible<std::decay_t<T>, Args...>>,
            int>
        = 0>
    std::decay_t<T>& emplace(Args&&... _args)
    {
        resetData();
        if constexpr (is_small_type<T>()) {
            auto& rval = reinterpret_cast<T&>(data_);
            return doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), std::forward<Args>(_args)...);
        } else {
            return doEmplaceBig<std::decay_t<T>>(std::forward<Args>(_args)...);
        }
    }
    template <class T, class E, class... Args,
        std::enable_if_t<std::conjunction_v<std::is_constructible<std::decay_t<T>, std::initializer_list<E>&, Args...>>, int> = 0>
    std::decay_t<T>& emplace(std::initializer_list<E> _ilist, Args&&... _args)
    {
        resetData();
        if constexpr (is_small_type<T>()) {
            auto& rval = reinterpret_cast<T&>(data_);
            return doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), _ilist, std::forward<Args>(_args)...);
        } else {
            return doEmplaceBig<std::decay_t<T>>(_ilist, std::forward<Args>(_args)...);
        }
    }

    ThisT& operator=(const EventBase& _other)
    {
        reset(_other);
        doCopyFrom(data_, smallCapacity(), smallAlign(), _other);
        return *this;
    }

    ThisT& operator=(EventBase&& _other)
    {
        reset(_other);
        doMoveFrom(data_, smallCapacity(), smallAlign(), _other);
        return *this;
    }
};

//-----------------------------------------------------------------------------
//      make_event
//-----------------------------------------------------------------------------
template <class Events, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
inline auto make_event(const Events _id)
{
    return Event<>(_id);
}

template <class Events, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
inline auto make_event(const EventCategoryBase& _category, const Events _id)
{
    return Event<>(_category, _id);
}

template <class Events, typename T, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
inline auto make_event(const Events _id, T&& _data)
{
    return Event<sizeof(std::decay_t<T>), alignof(std::decay_t<T>)>(_id, std::forward<T>(_data));
}

template <class Events, class T, class... Args, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
auto make_event(const Events _id, Args&&... _args)
{
    return Event<sizeof(std::decay_t<T>), alignof(std::decay_t<T>)>{_id, std::in_place_type<T>, std::forward<Args>(_args)...};
}
template <class Events, class T, class E, class... Args, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
auto make_event(const Events _id, std::initializer_list<E> _ilist, Args&&... _args)
{
    return Event<sizeof(std::decay_t<T>), alignof(std::decay_t<T>)>{_id, std::in_place_type<T>, _ilist, std::forward<Args>(_args)...};
}

template <class Events, typename T, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
inline auto make_event(const EventCategoryBase& _category, const Events _id, T&& _data)
{
    return Event<sizeof(std::decay_t<T>), alignof(std::decay_t<T>)>(_category, _id, std::forward<T>(_data));
}

template <class Events, class T, class... Args, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
auto make_event(const EventCategoryBase& _category, const Events _id, Args&&... _args)
{
    return Event<sizeof(std::decay_t<T>), alignof(std::decay_t<T>)>{_category, _id, std::in_place_type<T>, std::forward<Args>(_args)...};
}
template <class Events, class T, class E, class... Args, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
auto make_event(const EventCategoryBase& _category, const Events _id, std::initializer_list<E> _ilist, Args&&... _args)
{
    return Event<sizeof(std::decay_t<T>), alignof(std::decay_t<T>)>{_category, _id, std::in_place_type<T>, _ilist, std::forward<Args>(_args)...};
}

//-----------------------------------------------------------------------------
template <GenericEventE GenEvt>
inline const Event<0> generic_event = make_event(GenEvt);
//-----------------------------------------------------------------------------

inline bool EventBase::operator==(const EventBase& _rother) const
{
    return (pcategory_ == _rother.pcategory_) && (id_ == _rother.id_);
}

inline bool EventBase::empty() const
{
    return *this == generic_event<GenericEventE::Default>;
}

inline void EventBase::reset()
{
    pcategory_ = &category<GenericEventE>;
    id_        = to_underlying(GenericEventE::Default);
    resetData();
}

//-----------------------------------------------------------------------------
//      EventHandlerBase
//-----------------------------------------------------------------------------

class EventHandlerBase {
protected:
    const EventCategoryBase* eventCategory(const EventBase& _revt) const
    {
        return _revt.pcategory_;
    }

    uintptr_t eventId(const EventBase& _revt) const
    {
        return _revt.id_;
    }
};

//-----------------------------------------------------------------------------
//      EventHandler
//-----------------------------------------------------------------------------

template <typename RetVal, typename... Args>
class EventHandler : protected EventHandlerBase {
public:
    // using FunctionT = solid_function_t(RetVal(Event&, Args...));
    using FunctionT = std::function<RetVal(EventBase&, Args...)>;

private:
    using FunctionVectorT = std::vector<FunctionT>;
    using SizeTPairT      = std::pair<size_t, size_t>;
    using TypeIndexMapT   = std::unordered_map<std::type_index, SizeTPairT>;

public:
    struct InitItem {
        const Event<> evt;
        FunctionT     fnc;

        template <typename F>
        InitItem(const Event<>& _uevt, F&& _rf)
            : evt(_uevt)
            , fnc(std::cref(_rf))
        {
        }
    };

    template <typename F>
    EventHandler(
        F&&                             _rf,
        std::initializer_list<InitItem> init_lst)
        : invalid_event_fnc_(std::cref(_rf))
    {
        {
            size_t vec_size = 0;
            for (const InitItem& it : init_lst) {
                const std::type_index category_type_index{typeid(*eventCategory(it.evt))};
                auto                  map_it = category_map_.find(category_type_index);

                if (map_it != category_map_.end()) {
                    if (map_it->second.second < eventId(it.evt)) {
                        vec_size += (eventId(it.evt) - map_it->second.second);
                        map_it->second.second = eventId(it.evt);
                    }
                } else {
                    category_map_[category_type_index] = SizeTPairT(0, eventId(it.evt));
                    vec_size += (eventId(it.evt) + 1);
                }
            }

            function_vec_.resize(vec_size);
        }

        {
            size_t crt_off = 0;
            for (auto it = category_map_.begin(); it != category_map_.end(); ++it) {
                size_t skip      = it->second.second + 1;
                it->second.first = crt_off;
                crt_off += skip;
            }
        }

        for (const InitItem& it : init_lst) {
            const std::type_index category_type_index{typeid(*eventCategory(it.evt))};
            SizeTPairT&           categ_pair = category_map_[category_type_index];

            FunctionT& f = function_vec_[categ_pair.first + eventId(it.evt)];

            f = it.fnc;
        }
    }

    RetVal handle(EventBase& _revt, Args... args) const
    {
        const std::type_index category_type_index{typeid(*eventCategory(_revt))};
        auto                  map_it = category_map_.find(category_type_index);
        if (map_it != category_map_.end()) {
            const SizeTPairT& categ_pair = map_it->second;
            if (eventId(_revt) <= categ_pair.second) {
                const FunctionT& rfnc = function_vec_[categ_pair.first + eventId(_revt)];
                if (rfnc) {
                    return rfnc(_revt, args...);
                }
            }
        }
        return invalid_event_fnc_(_revt, args...);
    }

private:
    EventHandler(const EventHandler&)            = delete;
    EventHandler(EventHandler&&)                 = delete;
    EventHandler& operator=(const EventHandler&) = delete;
    EventHandler& operator=(EventHandler&&)      = delete;

private:
    FunctionT       invalid_event_fnc_;
    TypeIndexMapT   category_map_;
    FunctionVectorT function_vec_;
};

} // namespace solid
