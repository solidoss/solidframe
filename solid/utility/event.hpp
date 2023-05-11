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

class EventBase{
    friend class EventHandlerBase;

    const EventCategoryBase* pcategory_;
    uintptr_t                id_;
    uintptr_t type_data_ = 0;
    union{
        const any_impl::SmallRTTI* psmall_;
        const any_impl::BigRTTI* pbig_;
    }   rtti_;
    void*     pdata_;
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
        pdata_ = nullptr;
        type_data_ = 0;
    }

    bool hasData() const noexcept
    {
        return type_data_ != 0;
    }

    bool operator==(const EventBase& _rother) const;

    bool empty()const;

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
        return const_cast<T*>(static_cast<const ThisT*>(this)->get_if<T>());
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
        const EventCategoryBase &_rcategory, 
        const uintptr_t _id
    ):pcategory_(&_rcategory), id_(_id){}


    ~EventBase(){
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
    EventCategoryBase(const std::string& _name)
        : name_(_name)
    {
    }

    virtual ~EventCategoryBase() {}
private:
    virtual std::string_view eventName(const EventBase& _revt) const = 0;

private:
    const std::string name_;
};

//-----------------------------------------------------------------------------
//      EventCategory
//-----------------------------------------------------------------------------

template <typename EventIds>
class EventCategory : public EventCategoryBase {
    using FunctionT = std::function<std::string_view (const EventIds)>;

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
    Raise,
    Message,
    Timer,
    Pause,
    Resume,
    Update,
    Kill,
};

template <typename EventIds>
inline EventCategory<EventIds>  category;

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
        case GenericEventE::Raise:
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
    }
};

//-----------------------------------------------------------------------------
//      Event<>
//-----------------------------------------------------------------------------
template <size_t SmallSize = 0>
class Event;


template <>
class Event<0>: public EventBase{
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

    Event(): EventBase(category<GenericEventE>, to_underlying(GenericEventE::Default)){
    }

    template <class Events>
    Event(const Events _event): EventBase(category<Events>, _event){}

};

template <size_t SmallSize>
class Event: public EventBase{
    static constexpr size_t small_capacity = any_impl::compute_small_capacity(any_max(sizeof(void*), SmallSize));
    union {
        unsigned char  data_[small_capacity];
        max_align_t dummy_;
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

};

//-----------------------------------------------------------------------------
//      make_event
//-----------------------------------------------------------------------------
template <class Events>
inline Event<> make_event(const Events _id)
{
    return Event<>(_id);
}

template <class Events, typename T>
inline Event<sizeof(T)> make_event(const Events _id, T&& _uany_value)
{
    return Event<sizeof(T)>(_id, std::forward<T>(_uany_value));
}

template <GenericEventE Event>
inline const Event<0> generic_event = make_event(Event);
//-----------------------------------------------------------------------------

bool EventBase::operator==(const EventBase& _rother) const
{
    return (pcategory_ == _rother.pcategory_) && (id_ == _rother.id_);
}

bool EventBase::empty()const{
    return *this == generic_event<GenericEventE::Default>;
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
    using FunctionT = std::function<RetVal(Event&, Args...)>;

private:
    using FunctionVectorT = std::vector<FunctionT>;
    using SizeTPairT      = std::pair<size_t, size_t>;
    using TypeIndexMapT   = std::unordered_map<std::type_index, SizeTPairT>;

public:
    struct InitItem {
        const Event evt;
        FunctionT   fnc;

        template <typename F>
        InitItem(Event&& _uevt, F&& _rf)
            : evt(std::move(_uevt))
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

    RetVal handle(Event& _revt, Args... args) const
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
