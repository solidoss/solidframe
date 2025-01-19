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

#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "solid/system/exception.hpp"
#include "solid/utility/common.hpp"

namespace solid {
namespace reflection {
namespace v1 {

template <typename T>
class EnumMap : NonCopyable {
    static_assert(std::is_enum_v<T>, "T must be an enum");

    using DirectMapT  = std::unordered_map<T, std::string>;
    using ReverseMapT = std::unordered_map<std::string_view, T>;

    DirectMapT  direct_map_;
    ReverseMapT reverse_map_;

public:
    using EnumT     = T;
    using InitListT = std::initializer_list<std::pair<EnumT, std::string_view>>;

    EnumMap(InitListT _init_list)
    {
        for (const auto& item : _init_list) {
            const auto p = direct_map_.emplace(item.first, item.second);
            solid_check(p.second, "Enum with the same value already exists");
            reverse_map_.emplace(p.first->second, item.first);
        }
    }
    std::string_view get(const EnumT _key) const
    {
        const auto it = direct_map_.find(_key);
        if (it != direct_map_.end()) {
            return it->second;
        } else {
            return {};
        }
    }

    std::optional<EnumT> get(std::string_view _name) const
    {
        const auto it = reverse_map_.find(_name);
        if (it != reverse_map_.end()) {
            return it->second;
        } else {
            return {};
        }
    }
};

template <typename T>
extern EnumMap<T> enum_map_v;

#define enummap_instance(V) \
    template <>             \
    reflection::EnumMap<V> solid::reflection::v1::enum_map_v<V>

} // namespace v1
} // namespace reflection
} // namespace solid
