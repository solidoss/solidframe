// solid/reflection/v1/enummap.hpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once


#include <string_view>
#include <initializer_list>
#include <unordered_map>
#include <type_traits>
#include <string_view>

#include "solid/utility/common.hpp"

namespace solid{
namespace reflection{
namespace v1{

class EnumMap: NonCopyable{
    using DirectMapT = std::unordered_map<int64_t, std::string_view>;
    using ReverseMapT = std::unordered_map<std::string_view, int64_t>;
    
    DirectMapT direct_map_;
    ReverseMapT reverse_map_;
public:
    template <class T>
    using InitListT = std::initializer_list<std::pair<T, const char*>>;
    
    template <class T>
    EnumMap(TypeToType<T> _t, InitListT<T> _init_list){
        static_assert(std::is_enum_v<T>, "T must be an enum");
        
        for(const auto &item: _init_list){
            direct_map_.emplace(static_cast<int64_t>(static_cast<std::underlying_type_t<T>>(item.first)), item.second);
            reverse_map_.emplace(item.second, static_cast<int64_t>(static_cast<std::underlying_type_t<T>>(item.first)));
        }
    }
    template <class T>
    const char* get(const T _key)const{
        static_assert(std::is_enum_v<T>, "T must be an enum");
        const auto it = direct_map_.find(static_cast<int64_t>(static_cast<std::underlying_type_t<T>>(_key)));
        if(it != direct_map_.end()){
            return it->second.data();
        }else{
            return nullptr;
        }
    }
    
    template <class T>
    T get(std::string_view _name)const{
        static_assert(std::is_enum_v<T>, "T must be an enum");
        const auto it = reverse_map_.find(_name);
        if(it != reverse_map_.end()){
            return it->second;
        }else{
            return nullptr;
        }
    }
};

}//namespace v1
}//namespace reflection
}//namespace solid

