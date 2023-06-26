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
    uintptr_t                type_data_ = 0;
    union {
        const any_impl::SmallRTTI* psmall_;
        const any_impl::BigRTTI*   pbig_;
    } rtti_;
    void* pdata_;

public:
    std::ostream& print(std::ostream& _ros) const;

    void reset();

    void resetData()
    {
        switch (representation()) {
        case any_impl::RepresentationE::Small:
            rtti_.psmall_->pdestroy_fnc_(pdata_);
            break;
        case any_impl::RepresentationE::Big:
            rtti_.pbig_->pdestroy_fnc_(pdata_);
            break;
        case any_impl::RepresentationE::None:
        default:
            break;
        }
        pdata_     = nullptr;
        type_data_ = 0;
    }

    bool hasData() const noexcept
    {
        return type_data_ != 0;
    }

    bool operator==(const EventBase& _rother) const;

    bool empty() const;

    explicit operator bool() const noexcept
    {
        return !empty();
    }

    const std::type_info& type() const noexcept
    {
        const std::type_info* const pinfo = typeInfo();
        return pinfo ? *pinfo : typeid(void);
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
        switch (representation()) {
        case any_impl::RepresentationE::Small:
            return static_cast<const T*>(rtti_.psmall_->pget_if_fnc_(std::type_index(typeid(T)), pdata_));
        case any_impl::RepresentationE::Big:
            return static_cast<const T*>(rtti_.pbig_->pget_if_fnc_(std::type_index(typeid(T)), pdata_));
        default:
            return nullptr;
        }
    }

    template <class T>
    T* get_if() noexcept
    {
        return const_cast<T*>(static_cast<const EventBase*>(this)->get_if<T>());
    }

    bool is_movable() const
    {
        if (is_small()) {
            return rtti_.psmall_->is_movable_;
        } else if (is_big()) {
            return rtti_.pbig_->is_movable_;
        }
        return true;
    }
    bool is_copyable() const
    {
        if (is_small()) {
            return rtti_.psmall_->is_copyable_;
        } else if (is_big()) {
            return rtti_.pbig_->is_copyable_;
        }
        return true;
    }

    bool is_tuple() const
    {
        if (is_small()) {
            return rtti_.psmall_->is_tuple_;
        } else if (is_big()) {
            return rtti_.pbig_->is_tuple_;
        }
        return false;
    }

    bool is_small() const
    {
        return representation() == any_impl::RepresentationE::Small;
    }
    bool is_big() const
    {
        return representation() == any_impl::RepresentationE::Big;
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
    }

    ~EventBase()
    {
        reset();
    }

    any_impl::RepresentationE representation() const noexcept
    {
        return static_cast<any_impl::RepresentationE>(type_data_ & any_impl::representation_and_flags_mask);
    }

    void representation(const any_impl::RepresentationE _repr) noexcept
    {
        type_data_ &= (~any_impl::representation_and_flags_mask);
        type_data_ |= static_cast<uintptr_t>(_repr);
    }

    const std::type_info* typeInfo() const noexcept
    {
        return reinterpret_cast<const std::type_info*>(type_data_ & ~any_impl::representation_and_flags_mask);
    }

    void reset(const EventBase& _other)
    {
        pcategory_ = _other.pcategory_;
        id_        = _other.id_;
        resetData();
    }

    void doMoveFrom(void* _psmall_data, const size_t _small_capacity, EventBase& _other)
    {
        type_data_ = _other.type_data_;
        pdata_     = _psmall_data;
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
    }

    void doCopyFrom(void* _psmall_data, const size_t _small_capacity, const EventBase& _other)
    {
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
    }

    template <class T, class... Args>
    T& doEmplaceSmall(void* _psmall_data, const size_t _small_capacity, Args&&... _args)
    {
        auto* pdata   = ::new (_psmall_data) T{std::forward<Args>(_args)...};
        pdata_        = _psmall_data;
        rtti_.psmall_ = &any_impl::small_rtti<T>;
        type_data_    = reinterpret_cast<uintptr_t>(&typeid(T));
        representation(any_impl::RepresentationE::Small);

        return *pdata;
    }

    template <class T, class... Args>
    T& doEmplaceBig(Args&&... _args)
    {
        T* const ptr = ::new T(std::forward<Args>(_args)...);
        pdata_       = ptr;
        rtti_.pbig_  = &any_impl::big_rtti<T>;
        type_data_   = reinterpret_cast<uintptr_t>(&typeid(T));
        representation(any_impl::RepresentationE::Big);
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
template <size_t SmallSize = 0>
class Event;

template <class T>
struct is_event;

template <size_t V>
struct is_event<Event<V>> : std::true_type {
};

template <>
struct is_event<EventBase> : std::true_type {
};

template <class T>
struct is_event : std::false_type {
};

template <>
class Event<0> : public EventBase {
public:
    using ThisT = Event<0>;

    static constexpr size_t smallCapacity()
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

template <size_t SmallSize>
class Event : public EventBase {
    static constexpr size_t small_capacity = any_impl::compute_small_capacity(any_max(sizeof(void*), SmallSize));
    union {
        unsigned char data_[small_capacity];
        max_align_t   dummy_;
    };

public:
    using ThisT = Event<SmallSize>;

    static constexpr size_t smallCapacity()
    {
        return small_capacity;
    }

    template <class T>
    static constexpr bool is_small_type()
    {
        return alignof(T) <= alignof(max_align_t) && sizeof(T) <= small_capacity;
    }

    Event()
        : EventBase(category<GenericEventE>, to_underlying(GenericEventE::Default))
    {
    }

    Event(const ThisT& _other)
        : EventBase(_other)
    {
        doCopyFrom(data_, small_capacity, _other);
    }

    template <size_t Sz>
    Event(const Event<Sz>& _other)
        : EventBase(_other)
    {
        doCopyFrom(data_, small_capacity, _other);
    }

    Event(ThisT&& _other)
        : EventBase(_other)
    {
        doMoveFrom(data_, small_capacity, _other);
    }

    template <size_t Sz>
    Event(Event<Sz>&& _other)
        : EventBase(_other)
    {
        doMoveFrom(data_, small_capacity, _other);
    }

    template <typename Evs, class T, std::enable_if_t<std::conjunction_v<std::negation<is_event<std::decay_t<T>>>, std::negation<is_specialization<std::decay_t<T>, std::in_place_type_t>>>, int> = 0>
    Event(const Evs _ev, T&& _rvalue)
        : EventBase(category<Evs>, to_underlying(_ev))
    {

        if constexpr (is_small_type<T>()) {
            auto& rval = reinterpret_cast<T&>(data_);
            doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), small_capacity, std::forward<T>(_rvalue));
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
            doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), small_capacity, std::forward<T>(_rvalue));
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
            doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), small_capacity, std::forward<Args>(_args)...);
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
            doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), small_capacity, std::forward<Args>(_args)...);
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
            doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), small_capacity, _ilist, std::forward<Args>(_args)...);
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
            doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), small_capacity, _ilist, std::forward<Args>(_args)...);
        } else {
            doEmplaceBig<std::decay_t<T>>(_ilist, std::forward<Args>(_args)...);
        }
    }

    Event(const EventBase& _other)
        : EventBase(_other)
    {
        doCopyFrom(data_, small_capacity, _other);
    }

    Event(EventBase&& _other) noexcept
        : EventBase(_other)
    {
        doMoveFrom(data_, small_capacity, _other);
    }

    ThisT& operator=(const ThisT& _other)
    {
        *this = ThisT{_other};
        return *this;
    }

    ThisT& operator=(ThisT&& _other) noexcept
    {
        reset(_other);
        doMoveFrom(data_, small_capacity, _other);
        return *this;
    }

    template <size_t Sz>
    ThisT& operator=(const Event<Sz>& _other)
    {
        *this = ThisT{_other};
        return *this;
    }

    template <size_t Sz>
    ThisT& operator=(Event<Sz>&& _other) noexcept
    {
        reset(_other);
        doMoveFrom(data_, small_capacity, _other);
        return *this;
    }

    template <class T, std::enable_if_t<std::conjunction_v<std::negation<is_event<std::decay_t<T>>>, std::is_copy_constructible<std::decay_t<T>>>, int> = 0>
    ThisT& operator=(T&& _rvalue)
    {
        resetData();

        if constexpr (is_small_type<T>()) {
            auto& rval = reinterpret_cast<T&>(data_);
            doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), small_capacity, std::forward<T>(_rvalue));
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
            return doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), small_capacity, std::forward<Args>(_args)...);
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
            return doEmplaceSmall<std::decay_t<T>>(const_cast<void*>(static_cast<const volatile void*>(std::addressof(rval))), small_capacity, _ilist, std::forward<Args>(_args)...);
        } else {
            return doEmplaceBig<std::decay_t<T>>(_ilist, std::forward<Args>(_args)...);
        }
    }

    ThisT& operator=(const EventBase& _other)
    {
        reset(_other);
        doCopyFrom(data_, small_capacity, _other);
        return *this;
    }

    ThisT& operator=(EventBase&& _other)
    {
        reset(_other);
        doMoveFrom(data_, small_capacity, _other);
        return *this;
    }
};

//-----------------------------------------------------------------------------
//      make_event
//-----------------------------------------------------------------------------
template <class Events, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
inline Event<> make_event(const Events _id)
{
    return Event<>(_id);
}

template <class Events, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
inline Event<> make_event(const EventCategoryBase& _category, const Events _id)
{
    return Event<>(_category, _id);
}

template <class Events, typename T, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
inline Event<sizeof(std::decay_t<T>)> make_event(const Events _id, T&& _data)
{
    return Event<sizeof(std::decay_t<T>)>(_id, std::forward<T>(_data));
}

template <class Events, class T, class... Args, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
Event<sizeof(std::decay_t<T>)> make_event(const Events _id, Args&&... _args)
{
    return Event<sizeof(std::decay_t<T>)>{_id, std::in_place_type<T>, std::forward<Args>(_args)...};
}
template <class Events, class T, class E, class... Args, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
Event<sizeof(std::decay_t<T>)> make_event(const Events _id, std::initializer_list<E> _ilist, Args&&... _args)
{
    return Event<sizeof(std::decay_t<T>)>{_id, std::in_place_type<T>, _ilist, std::forward<Args>(_args)...};
}

template <class Events, typename T, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
inline Event<sizeof(std::decay_t<T>)> make_event(const EventCategoryBase& _category, const Events _id, T&& _data)
{
    return Event<sizeof(std::decay_t<T>)>(_category, _id, std::forward<T>(_data));
}

template <class Events, class T, class... Args, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
Event<sizeof(std::decay_t<T>)> make_event(const EventCategoryBase& _category, const Events _id, Args&&... _args)
{
    return Event<sizeof(std::decay_t<T>)>{_category, _id, std::in_place_type<T>, std::forward<Args>(_args)...};
}
template <class Events, class T, class E, class... Args, std::enable_if_t<std::is_enum_v<Events>, int> = 0>
Event<sizeof(std::decay_t<T>)> make_event(const EventCategoryBase& _category, const Events _id, std::initializer_list<E> _ilist, Args&&... _args)
{
    return Event<sizeof(std::decay_t<T>)>{_category, _id, std::in_place_type<T>, _ilist, std::forward<Args>(_args)...};
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
