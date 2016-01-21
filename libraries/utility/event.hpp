// frame/event.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_EVENT_HPP
#define SOLID_FRAME_EVENT_HPP

#include <ostream>
#include <typeinfo>
#include <typeindex>
#include <utility>

#include <queue>
#include <iostream>
#include <vector>
#include <functional>
#include <unordered_map>

#include "utility/any.hpp"

namespace solid{

class EventCategoryBase;
class EventHandlerBase;

//-----------------------------------------------------------------------------
//		Event
//-----------------------------------------------------------------------------


struct Event{
	static constexpr size_t any_size = AnyBase::min_data_size + sizeof(void*) + sizeof(uint64_t);
	
	using AnyT = Any<any_size>;
	
	Event();
	Event(Event &&);
	Event(const Event &);
	
	Event& operator=(const Event &);
	Event& operator=(Event&&);
	
	std::ostream& print(std::ostream &_ros)const;
	
	AnyT& any(){
		return any_;
	}
	
	const AnyT& any()const{
		return any_;
	}
	
	bool operator==(const Event &_revt)const;
	
	void clear();
	
	bool isDefault()const;
	
private:
	friend class EventCategoryBase;
	friend class EventHandlerBase;
	
	Event(
		const size_t _id,
		const EventCategoryBase &_rcategory
	):pcategory_(&_rcategory), id_(_id){}

private:
	const EventCategoryBase	*pcategory_;
	size_t					id_;
	AnyT					any_;
};

std::ostream& operator<<(std::ostream &_ros, Event const&_re);

//-----------------------------------------------------------------------------
//		EventCategoryBase
//-----------------------------------------------------------------------------


class EventCategoryBase{
public:
	const std::string& name()const{
		return name_;
	}
protected:
	
	EventCategoryBase(const std::string &_name):name_(_name){}
	
	virtual ~EventCategoryBase(){}
	
	Event event(const size_t _idx)const{
		return Event(_idx, *this);
	}
	size_t eventId(const Event &_revt)const{
		return _revt.id_;
	}
private:
	friend class Event;
	virtual const char* name(const Event &_revt)const = 0;
private:
	std::string name_;
};

//-----------------------------------------------------------------------------
//		EventCategory
//-----------------------------------------------------------------------------

template <typename EventIds>
class EventCategory: public EventCategoryBase{
	using FunctionT = std::function<const char*(const EventIds)>;
public:
	template <typename F>
	EventCategory(const std::string &_name, F _f):EventCategoryBase(_name), names_fnc_(_f){}
	
	Event event(const EventIds _id)const{
		return EventCategoryBase::event(static_cast<size_t>(_id));
	}
private:
	virtual const char* name(const Event &_revt)const override{
		return names_fnc_(static_cast<EventIds>(eventId(_revt)));
	}
private:
	FunctionT	names_fnc_;
};

enum class GenericEvents{
	Default,
	Start,
	Stop,
	Raise,
	Message,
	Timer,
	Kill,
};

extern const EventCategory<GenericEvents>	generic_event_category;

//-----------------------------------------------------------------------------
//		EventHandlerBase
//-----------------------------------------------------------------------------

class EventHandlerBase{
protected:
	const EventCategoryBase* eventCategory(const Event &_revt)const{
		return _revt.pcategory_;
	}
	
	size_t eventId(const Event &_revt)const{
		return _revt.id_;
	}
};

//-----------------------------------------------------------------------------
//		EventHandler
//-----------------------------------------------------------------------------

template <typename RetVal, typename ...Args>
class EventHandler: protected EventHandlerBase{
public:
	using FunctionT = std::function<RetVal(Event&, Args...)>;
private:
	using FunctionVectorT = std::vector<FunctionT>;
	using SizeTPairT = std::pair<size_t, size_t>;
	using TypeIndexMapT = std::unordered_map<std::type_index, SizeTPairT>;
public:
	struct InitItem{
		const Event		evt;
		FunctionT		fnc;
		
		template <typename F>
		InitItem(Event && _uevt, F &&_rf):evt(std::move(_uevt)), fnc(std::cref(_rf)){}
	};
	
	template <typename F>
	EventHandler(
		F &&_rf,
		std::initializer_list<InitItem> init_lst
	): invalid_event_fnc_(std::cref(_rf))
	{
		{
			size_t		vec_size = 0;
			for(const InitItem &it:init_lst){
				const std::type_index	 category_type_index{typeid(*eventCategory(it.evt))};
				auto 					 map_it = category_map_.find(category_type_index);
				
				if(map_it != category_map_.end()){
					if(map_it->second.second < eventId(it.evt)){
						vec_size += (eventId(it.evt) - map_it->second.second);
						map_it->second.second = eventId(it.evt);
					}
				}else{
					category_map_[category_type_index] = SizeTPairT(0, eventId(it.evt));
					vec_size += (eventId(it.evt) + 1);
				}
			}
			
			function_vec_.resize(vec_size);
		}
		
		{
			size_t	crt_off = 0;
			for(auto it = category_map_.begin(); it != category_map_.end(); ++it){
				size_t					skip = it->second.second + 1;
				it->second.first = crt_off;
				crt_off += skip;
			}
		}
		
		for(const InitItem &it:init_lst){
			const std::type_index	category_type_index{typeid(*eventCategory(it.evt))};
			SizeTPairT 				&categ_pair = category_map_[category_type_index];
			
			function_vec_[categ_pair.first + eventId(it.evt)] = std::move(it.fnc);
		}
	}
	
	RetVal handle(Event &_revt, Args...args)const{
		const std::type_index	category_type_index{typeid(*eventCategory(_revt))};
		auto 					map_it = category_map_.find(category_type_index);
		if(map_it != category_map_.end()){
			const SizeTPairT 	&categ_pair = map_it->second;
			if(eventId(_revt) <= categ_pair.second){
				const FunctionT &rfnc = function_vec_[categ_pair.first + eventId(_revt)];
				if(rfnc != nullptr){
					return rfnc(_revt, args...);
				}
			}
		}
		return invalid_event_fnc_(_revt, args...);
	}
private:
	EventHandler(const EventHandler&) = delete;
	EventHandler(EventHandler&&) = delete;
	EventHandler& operator=(const EventHandler&) = delete;
	EventHandler& operator=(EventHandler&&) = delete;
private:
	FunctionT		invalid_event_fnc_;
	TypeIndexMapT	category_map_;
	FunctionVectorT	function_vec_;
};


}//namespace solid

#endif
