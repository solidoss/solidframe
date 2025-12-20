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
// #define SOLID_THROW_ON_BIG_EVENT

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
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return any_impl::BaseRTTI::get(rtti).is_movable_;
        }
        return true;
    }
    bool is_copyable() const
    {
        auto const rtti = rtti_;
        if (rtti) [[likely]] {
            return any_impl::BaseRTTI::get(rtti).is_copyable_;
        }
        return true;
    }

    bool is_tuple() const
    {
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
    }

    template <class T, class... Args>
    T& doEmplaceSmall(void* _psmall_data, Args&&... _args)
    {
        auto* pdata = ::new (_psmall_data) T{std::forward<Args>(_args)...};
        pdata_      = _psmall_data;
        rtti_       = any_impl::representation(&any_impl::small_rtti<T>, any_impl::RepresentationE::Small);
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
        return *ptr;
    }
};

std::ostream& operator<<(std::ostream& _ros, EventBase const& _re);

//-----------------------------------------------------------------------------
//      EventCategoryBase
//-----------------------------------------------------------------------------

class EventCategoryBase {
public:
    [[nodiscard]] const std::string& name() const
    {
        return name_;
    }

protected:
    EventCategoryBase(const std::string_view& _name)
        : name_(_name)
    {
    }

    virtual ~EventCategoryBase() {}

    [[nodiscard]] uintptr_t eventId(const EventBase& _revt) const
    {
        return _revt.id_;
    }

private:
    friend class EventBase;
    [[nodiscard]] virtual std::string_view eventName(const EventBase& _revt) const = 0;

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
template <
    size_t      SmallSize  = 0,
    size_t      SmallAlign = SmallSize == 0 ? 0 : sizeof(uintptr_t),
    StoreOption Option     = StoreOption::AcceptBig>
    requires((SmallSize == 0 and SmallAlign == 0) or (SmallSize > 0 and SmallSize >= SmallAlign and (SmallSize % SmallAlign == 0) and std::popcount(SmallAlign) == 1))
class Event;

template <class T>
struct is_event;

template <size_t V, size_t A, StoreOption O>
struct is_event<Event<V, A, O>> : std::true_type {
};

template <>
struct is_event<EventBase> : std::true_type {
};

template <class T>
struct is_event : std::false_type {
};

template <class T>
inline constexpr bool is_event_v = is_event<T>::value;

template <>
class Event<0, 0, StoreOption::AcceptBig> : public EventBase {
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

template <size_t SmallSize, size_t SmallAlign, StoreOption Option>
    requires((SmallSize == 0 and SmallAlign == 0) or (SmallSize > 0 and SmallSize >= SmallAlign and (SmallSize % SmallAlign == 0) and std::popcount(SmallAlign) == 1))
class Event : public EventBase {
    alignas(SmallAlign) unsigned char data_[SmallSize];

public:
    using ThisT = Event<SmallSize, SmallAlign, Option>;

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

    template <size_t Sz, size_t Al, StoreOption Op>
    Event(const Event<Sz, Al, Op>& _other)
        : EventBase(_other)
    {
        doCopyFrom(data_, smallCapacity(), smallAlign(), _other);
    }

    Event(ThisT&& _other)
        : EventBase(_other)
    {
        doMoveFrom(data_, smallCapacity(), smallAlign(), _other);
    }

    template <size_t Sz, size_t Al, StoreOption Op>
    Event(Event<Sz, Al, Op>&& _other)
        : EventBase(_other)
    {
        doMoveFrom(data_, smallCapacity(), smallAlign(), _other);
    }

    template <typename Evs, class T, StoreOption Opt = Option>
    Event(
        const Evs _ev, T&& _rvalue,
        std::integral_constant<StoreOption, Opt> = store_option_dispatch<Opt>())
        requires(not is_event_v<std::decay_t<T>> and not is_specialization_v<std::decay_t<T>, std::in_place_type_t>)
        : EventBase(category<Evs>, to_underlying(_ev))
    {
        using ValueT = std::decay_t<T>;
        static_assert(Opt == StoreOption::AcceptBig or is_small_type<ValueT>(), "Value not small. Construct by using AcceptBigT{} or assign using .emplace()");
        if constexpr (is_small_type<ValueT>()) {
            auto& rval = reinterpret_cast<ValueT&>(data_);
            doEmplaceSmall<ValueT>(std::addressof(rval), std::forward<T>(_rvalue));
        } else {
            doEmplaceBig<ValueT>(std::forward<>(_rvalue));
        }
    }

    template <typename Evs, class T, StoreOption Opt = Option>
    Event(const EventCategoryBase& _category, const Evs _ev,
        T&& _rvalue, std::integral_constant<StoreOption, Opt> = store_option_dispatch<Opt>())
        requires(not is_event_v<std::decay_t<T>> and not is_specialization_v<std::decay_t<T>, std::in_place_type_t>)
        : EventBase(_category, to_underlying(_ev))
    {
        using ValueT = std::decay_t<T>;
        static_assert(Opt == StoreOption::AcceptBig or is_small_type<ValueT>(), "Value not small. Construct by using AcceptBigT{} or assign using .emplace()");
        if constexpr (is_small_type<ValueT>()) {
            auto& rval = reinterpret_cast<ValueT&>(data_);
            doEmplaceSmall<ValueT>(std::addressof(rval), std::forward<T>(_rvalue));
        } else {
            doEmplaceBig<ValueT>(std::forward<T>(_rvalue));
        }
    }

    template <typename Evs, class T, class... Args, StoreOption Opt = Option>
    explicit Event(const Evs _ev, std::in_place_type_t<T>, Args&&... _args,
        std::integral_constant<StoreOption, Opt> = store_option_dispatch<Opt>())
        requires(std::is_constructible_v<std::decay_t<T>, Args...>)
        : EventBase(category<Evs>, to_underlying(_ev))
    {
        using ValueT = std::decay_t<T>;
        static_assert(Opt == StoreOption::AcceptBig or is_small_type<ValueT>(), "Value not small. Construct by using AcceptBigT{} or assign using .emplace()");
        if constexpr (is_small_type<ValueT>()) {
            auto& rval = reinterpret_cast<ValueT&>(data_);
            doEmplaceSmall<ValueT>(std::addressof(rval), std::forward<Args>(_args)...);
        } else {
            doEmplaceBig<ValueT>(std::forward<Args>(_args)...);
        }
    }

    template <typename Evs, class T, class... Args, StoreOption Opt = Option>
    explicit Event(const EventCategoryBase& _category, const Evs _ev, std::in_place_type_t<T>,
        Args&&... _args, std::integral_constant<StoreOption, Opt> = store_option_dispatch<Opt>())
        requires(std::is_constructible_v<std::decay_t<T>, Args...>)
        : EventBase(_category, to_underlying(_ev))
    {
        using ValueT = std::decay_t<T>;
        static_assert(Opt == StoreOption::AcceptBig or is_small_type<ValueT>(), "Value not small. Construct by using AcceptBigT{} or assign using .emplace()");
        if constexpr (is_small_type<ValueT>()) {
            auto& rval = reinterpret_cast<ValueT&>(data_);
            doEmplaceSmall<ValueT>(std::addressof(rval), std::forward<Args>(_args)...);
        } else {
            doEmplaceBig<ValueT>(std::forward<Args>(_args)...);
        }
    }

    template <typename Evs, class T, class E, class... Args, StoreOption Opt = Option>
    explicit Event(const Evs _ev, std::in_place_type_t<T>, std::initializer_list<E> _ilist,
        Args&&... _args, std::integral_constant<StoreOption, Opt> = store_option_dispatch<Opt>())
        requires(std::is_constructible_v<std::decay_t<T>, std::initializer_list<E>&, Args...> and std::is_copy_constructible_v<std::decay_t<T>>)
        : EventBase(category<Evs>, to_underlying(_ev))
    {
        using ValueT = std::decay_t<T>;
        static_assert(Opt == StoreOption::AcceptBig or is_small_type<ValueT>(), "Value not small. Construct by using AcceptBigT{} or assign using .emplace()");
        if constexpr (is_small_type<ValueT>()) {
            auto& rval = reinterpret_cast<ValueT&>(data_);
            doEmplaceSmall<ValueT>(std::addressof(rval), _ilist, std::forward<Args>(_args)...);
        } else {
            doEmplaceBig<ValueT>(_ilist, std::forward<Args>(_args)...);
        }
    }

    template <typename Evs, class T, class E, class... Args, StoreOption Opt = Option>
    explicit Event(const EventCategoryBase& _category, const Evs _ev, std::in_place_type_t<T>,
        std::initializer_list<E> _ilist, Args&&... _args, std::integral_constant<StoreOption, Opt> = store_option_dispatch<Opt>())
        requires(std::is_constructible_v<std::decay_t<T>, std::initializer_list<E>&, Args...> and std::is_copy_constructible_v<std::decay_t<T>>)
        : EventBase(_category, to_underlying(_ev))
    {
        using ValueT = std::decay_t<T>;
        static_assert(Opt == StoreOption::AcceptBig or is_small_type<ValueT>(), "Value not small. Construct by using AcceptBigT{} or assign using .emplace()");
        if constexpr (is_small_type<ValueT>()) {
            auto& rval = reinterpret_cast<ValueT&>(data_);
            doEmplaceSmall<ValueT>(std::addressof(rval), _ilist, std::forward<Args>(_args)...);
        } else {
            doEmplaceBig<ValueT>(_ilist, std::forward<Args>(_args)...);
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

    template <size_t Sz, size_t Al, StoreOption Op>
    ThisT& operator=(const Event<Sz, Al, Op>& _other)
    {
        *this = ThisT{_other};
        return *this;
    }

    template <size_t Sz, size_t Al, StoreOption Op>
    ThisT& operator=(Event<Sz, Al, Op>&& _other) noexcept
    {
        reset(_other);
        doMoveFrom(data_, smallCapacity(), smallAlign(), _other);
        return *this;
    }

    template <class T>
    ThisT& operator=(T&& _rvalue)
        requires(not is_event_v<std::decay_t<T>> and std::is_copy_constructible_v<std::decay_t<T>>)
    {
        using ValueT = std::decay_t<T>;
        static_assert(is_small_type<ValueT>(), "Value not small. Construct by using AcceptBigT{} or assign using .emplace()");

        resetData();

        if constexpr (is_small_type<ValueT>()) {
            auto& rval = reinterpret_cast<T&>(data_);
            doEmplaceSmall<ValueT>(std::addressof(rval), std::forward<T>(_rvalue));
        } else {
            doEmplaceBig<ValueT>(std::forward<T>(_rvalue));
        }
        return *this;
    }

    template <class T>
    ThisT& emplace(T&& _rvalue)
        requires(not is_event_v<std::decay_t<T>> and std::is_copy_constructible_v<std::decay_t<T>>)
    {
        using ValueT = std::decay_t<T>;
        resetData();

        if constexpr (is_small_type<ValueT>()) {
            auto& rval = reinterpret_cast<T&>(data_);
            doEmplaceSmall<ValueT>(std::addressof(rval), std::forward<T>(_rvalue));
        } else {
            doEmplaceBig<ValueT>(std::forward<T>(_rvalue));
        }
        return *this;
    }

    template <class T, class... Args>
    ThisT& emplace(Args&&... _args)
        requires(std::is_constructible_v<std::decay_t<T>, Args...>)
    {
        resetData();
        using ValueT = std::decay_t<T>;
        if constexpr (is_small_type<ValueT>()) {
            auto& rval = reinterpret_cast<ValueT&>(data_);
            doEmplaceSmall<ValueT>(std::addressof(rval), std::forward<Args>(_args)...);
        } else {
            doEmplaceBig<ValueT>(std::forward<Args>(_args)...);
        }
        return *this;
    }

    template <class T, class E, class... Args>
    ThisT& emplace(std::initializer_list<E> _ilist, Args&&... _args)
        requires(std::is_constructible_v<std::decay_t<T>, std::initializer_list<E>&, Args...>)
    {
        resetData();
        using ValueT = std::decay_t<T>;
        if constexpr (is_small_type<ValueT>()) {
            auto& rval = reinterpret_cast<ValueT&>(data_);
            doEmplaceSmall<ValueT>(std::addressof(rval), _ilist, std::forward<Args>(_args)...);
        } else {
            doEmplaceBig<ValueT>(_ilist, std::forward<Args>(_args)...);
        }
        return *this;
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
template <class Events>
auto make_event(const Events _id)
    requires(std::is_enum_v<Events>)
{
    return Event<>(_id);
}

template <class Events>
auto make_event(const EventCategoryBase& _category, const Events _id)
    requires(std::is_enum_v<Events>)
{
    return Event<>(_category, _id);
}

template <class Events, typename T>
auto make_event(const Events _id, T&& _data)
    requires(std::is_enum_v<Events>)
{
    return Event<sizeof(std::decay_t<T>), alignof(std::decay_t<T>), StoreOption::RejectBig>(_id, std::forward<T>(_data));
}

template <class Events, class T, class... Args>
auto make_event(const Events _id, Args&&... _args)
    requires(std::is_enum_v<Events>)
{
    return Event<sizeof(std::decay_t<T>), alignof(std::decay_t<T>), StoreOption::RejectBig>{_id, std::in_place_type<T>, std::forward<Args>(_args)...};
}
template <class Events, class T, class E, class... Args>
auto make_event(const Events _id, std::initializer_list<E> _ilist, Args&&... _args)
    requires(std::is_enum_v<Events>)
{
    return Event<sizeof(std::decay_t<T>), alignof(std::decay_t<T>), StoreOption::RejectBig>{_id, std::in_place_type<T>, _ilist, std::forward<Args>(_args)...};
}

template <class Events, typename T>
auto make_event(const EventCategoryBase& _category, const Events _id, T&& _data)
    requires(std::is_enum_v<Events>)
{
    return Event<sizeof(std::decay_t<T>), alignof(std::decay_t<T>), StoreOption::RejectBig>(_category, _id, std::forward<T>(_data));
}

template <class Events, class T, class... Args>
auto make_event(const EventCategoryBase& _category, const Events _id, Args&&... _args)
    requires(std::is_enum_v<Events>)
{
    return Event<sizeof(std::decay_t<T>), alignof(std::decay_t<T>), StoreOption::RejectBig>{_category, _id, std::in_place_type<T>, std::forward<Args>(_args)...};
}
template <class Events, class T, class E, class... Args>
auto make_event(const EventCategoryBase& _category, const Events _id, std::initializer_list<E> _ilist, Args&&... _args)
    requires(std::is_enum_v<Events>)
{
    return Event<sizeof(std::decay_t<T>), alignof(std::decay_t<T>), StoreOption::RejectBig>{_category, _id, std::in_place_type<T>, _ilist, std::forward<Args>(_args)...};
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
    using FunctionT = Function<RetVal(EventBase&, Args...)>;

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
