// solid/event.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <ostream>
#include <typeindex>
#include <typeinfo>
#include <utility>

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
//      Event
//-----------------------------------------------------------------------------

struct Event {
    static constexpr size_t any_size = any_min_data_size + max_size(sizeof(void*) + sizeof(uint64_t), sizeof(std::shared_ptr<uint64_t>));

    using AnyT = Any<any_size>;

    Event();
    Event(Event&&);
    Event(const Event&);

    Event& operator=(const Event&);
    Event& operator=(Event&&);

    std::ostream& print(std::ostream& _ros) const;

    AnyT& any()
    {
        return any_;
    }

    const AnyT& any() const
    {
        return any_;
    }

    bool operator==(const Event& _revt) const;

    void clear();

    bool isDefault() const;

    bool empty() const
    {
        return isDefault();
    }

private:
    friend class EventCategoryBase;
    friend class EventHandlerBase;

    Event(
        const size_t             _id,
        const EventCategoryBase& _rcategory)
        : pcategory_(&_rcategory)
        , id_(_id)
    {
    }

    template <class T>
    explicit Event(
        const size_t             _id,
        const EventCategoryBase& _rcategory,
        const T&                 _rany_value)
        : pcategory_(&_rcategory)
        , id_(_id)
        , any_(_rany_value)
    {
    }

    template <class T>
    explicit Event(
        const size_t             _id,
        const EventCategoryBase& _rcategory,
        T&&                      _uany_value)
        : pcategory_(&_rcategory)
        , id_(_id)
        , any_(std::move(_uany_value))
    {
    }

private:
    const EventCategoryBase* pcategory_;
    size_t                   id_;
    AnyT                     any_;
};

std::ostream& operator<<(std::ostream& _ros, Event const& _re);

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

    Event event(const size_t _idx) const
    {
        return Event(_idx, *this);
    }

    template <typename T>
    Event event(const size_t _idx, const T& _rany_value) const
    {
        return Event(_idx, *this, _rany_value);
    }

    template <typename T>
    Event event(const size_t _idx, T&& _uany_value) const
    {
        return Event(_idx, *this, std::move(_uany_value));
    }

    size_t eventId(const Event& _revt) const
    {
        return _revt.id_;
    }

private:
    friend struct Event;
    virtual const char* name(const Event& _revt) const = 0;

private:
    std::string name_;
};

//-----------------------------------------------------------------------------
//      EventCategory
//-----------------------------------------------------------------------------

template <typename EventIds>
class EventCategory : public EventCategoryBase {
    using FunctionT = solid_function_t(const char*(const EventIds));

public:
    template <typename F>
    EventCategory(const std::string& _name, F _f)
        : EventCategoryBase(_name)
        , names_fnc_(std::move(_f))
    {
    }

    Event event(const EventIds _id) const
    {
        return EventCategoryBase::event(static_cast<size_t>(_id));
    }

    template <typename T>
    Event event(const EventIds _id, const T& _rany_value) const
    {
        return EventCategoryBase::event(static_cast<size_t>(_id), _rany_value);
    }

    template <typename T>
    Event event(const EventIds _id, T&& _uany_value) const
    {
        return EventCategoryBase::event(static_cast<size_t>(_id), std::move(_uany_value));
    }

private:
    virtual const char* name(const Event& _revt) const override
    {
        return names_fnc_(static_cast<EventIds>(eventId(_revt)));
    }

private:
    FunctionT names_fnc_;
};

enum class GenericEvents {
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

extern const EventCategory<GenericEvents> generic_event_category;

inline Event make_event(const GenericEvents _id)
{
    return generic_event_category.event(_id);
}

template <typename T>
inline Event make_event(const GenericEvents _id, const T& _rany_value)
{
    return generic_event_category.event(_id, _rany_value);
}

template <typename T>
inline Event make_event(const GenericEvents _id, T&& _uany_value)
{
    return generic_event_category.event(_id, _uany_value);
}
//-----------------------------------------------------------------------------

inline Event::Event()
    : pcategory_(&generic_event_category)
    , id_(static_cast<size_t>(GenericEvents::Default))
{
}

inline Event::Event(Event&& _uevt)
    : pcategory_(_uevt.pcategory_)
    , id_(_uevt.id_)
    , any_(std::move(_uevt.any_))
{
    _uevt.pcategory_ = &generic_event_category;
    _uevt.id_        = static_cast<size_t>(GenericEvents::Default);
}

inline Event::Event(const Event& _revt)
    : pcategory_(_revt.pcategory_)
    , id_(_revt.id_)
    , any_(_revt.any_)
{
}

inline Event& Event::operator=(const Event& _revt)
{
    pcategory_ = _revt.pcategory_;
    id_        = _revt.id_;
    any_       = _revt.any_;
    return *this;
}

inline Event& Event::operator=(Event&& _uevt)
{
    pcategory_       = _uevt.pcategory_;
    id_              = _uevt.id_;
    any_             = std::move(_uevt.any_);
    _uevt.pcategory_ = &generic_event_category;
    _uevt.id_        = static_cast<size_t>(GenericEvents::Default);
    return *this;
}

inline void Event::clear()
{
    pcategory_ = &generic_event_category;
    id_        = static_cast<size_t>(GenericEvents::Default);
    any_.clear();
}

inline bool Event::isDefault() const
{
    return pcategory_ == &generic_event_category && id_ == static_cast<size_t>(GenericEvents::Default);
}

inline bool Event::operator==(const Event& _revt) const
{
    return (pcategory_ == _revt.pcategory_) && (id_ == _revt.id_);
}

//-----------------------------------------------------------------------------

extern const Event generic_event_default;
extern const Event generic_event_start;
extern const Event generic_event_stop;
extern const Event generic_event_raise;
extern const Event generic_event_message;
extern const Event generic_event_timer;
extern const Event generic_event_pause;
extern const Event generic_event_resume;
extern const Event generic_event_update;
extern const Event generic_event_kill;

//-----------------------------------------------------------------------------
//      EventHandlerBase
//-----------------------------------------------------------------------------

class EventHandlerBase {
protected:
    const EventCategoryBase* eventCategory(const Event& _revt) const
    {
        return _revt.pcategory_;
    }

    size_t eventId(const Event& _revt) const
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
    using FunctionT = solid_function_t(RetVal(Event&, Args...));

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
    EventHandler(const EventHandler&) = delete;
    EventHandler(EventHandler&&)      = delete;
    EventHandler& operator=(const EventHandler&) = delete;
    EventHandler& operator=(EventHandler&&) = delete;

private:
    FunctionT       invalid_event_fnc_;
    TypeIndexMapT   category_map_;
    FunctionVectorT function_vec_;
};

} //namespace solid
