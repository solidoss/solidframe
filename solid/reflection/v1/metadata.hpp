// solid/reflection/v1/metadata.hpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#pragma once

#include <variant>
#include <limits>
#include <functional>

#include "solid/reflection/v1/typetraits.hpp"
#include "solid/reflection/v1/typemap.hpp"
#include "solid/reflection/v1/enummap.hpp"

namespace solid{
namespace reflection{
namespace v1{
namespace metadata{
struct Generic{
};

struct Enum{
    const EnumMap *pmap_ = nullptr;
    
    Enum(){}
    
    Enum(const EnumMap& _rmap):pmap_(&_rmap){}
    
    auto& map(const EnumMap& _rmap) {
        pmap_ = &_rmap;
        return *this;
    }
    
    const EnumMap* map()const{
        return pmap_;
    }
};

struct Pointer{
    const TypeMapBase *pmap_ = nullptr;
    
    Pointer(){}
    
    Pointer(const TypeMapBase* _pmap):pmap_(_pmap){}
    
    auto& map(const TypeMapBase& _rmap) {
        pmap_ = &_rmap;
        return *this;
    }
    
    const TypeMapBase* map()const{
        return pmap_;
    }
};

struct SignedInteger{
    int64_t           min_ = std::numeric_limits<int64_t>::min();
    int64_t           max_ = std::numeric_limits<int64_t>::max();
    
    auto& min(const int64_t &_min){
        min_ = _min;
        return *this;
    }
    
    auto& max(const int64_t &_max){
        max_ = _max;
        return *this;
    } 
};

struct UnsignedInteger{
    uint64_t           min_ = 0;
    uint64_t           max_ = std::numeric_limits<uint64_t>::max();
    
    auto& min(const uint64_t &_min){
        min_ = _min;
        return *this;
    }
    
    auto& max(const uint64_t &_max){
        max_ = _max;
        return *this;
    } 
};

struct String{
    size_t      max_size_ = -1ULL;
    size_t      min_size_ = 0;
    bool        is_sensitive_ = false;
    
    String(
        const size_t _max_size = -1ULL,
        const size_t _min_size = 0,
        const bool _is_sensitive = false
    ): max_size_(_max_size), min_size_(_min_size), is_sensitive_(_is_sensitive){}
    
    auto& maxSize(const size_t _max_size) {
        max_size_ = _max_size;
        return *this;
    }
    auto& minSize(const size_t _min_size) {
        min_size_ = _min_size;
        return *this;
    }
    auto& sesitive(const bool _is_sensitive){
        is_sensitive_ = _is_sensitive;
        return *this;
    }
};

struct Container{
    size_t      max_size_ = -1ULL;
    size_t      min_size_ = 0;
    
    Container(
        const size_t _max_size = -1ULL,
        const size_t _min_size = 0
    ): max_size_(_max_size), min_size_(_min_size){}
    
    auto& maxSize(const size_t _max_size) {
        max_size_ = _max_size;
        return *this;
    }
    auto& minSize(const size_t _min_size) {
        min_size_ = _min_size;
        return *this;
    }
};

struct IStream{
    using ProgressFunctionT = std::function<void(std::istream&, uint64_t, const bool, const size_t, const char*)>;//std::istream& _ris, uint64_t _len, const bool _done, const size_t _index, const char* _name
    uint64_t max_size_ = std::numeric_limits<uint64_t>::max();
    ProgressFunctionT progress_function_;
    
    auto& maxSize(const uint64_t _max_size) {
        max_size_ = _max_size;
        return *this;
    }
    
    template <class ProgressF>
    auto& progressFunction(ProgressF _fnc){
        progress_function_ = _fnc;
        return *this;
    }
};

struct OStream{
    using ProgressFunctionT = std::function<void(std::ostream&, uint64_t, const bool, const size_t, const char*)>;//std::istream& _ris, uint64_t _len, const bool _done, const size_t _index, const char* _name
    uint64_t max_size_ = std::numeric_limits<uint64_t>::max();
    ProgressFunctionT progress_function_;
    
    auto& maxSize(const uint64_t _max_size) {
        max_size_ = _max_size;
        return *this;
    }
    
    template <class ProgressF>
    auto& progressFunction(ProgressF _fnc){
        progress_function_ = _fnc;
        return *this;
    }
};

struct Variant{
    using VariantT = std::variant<
    Generic, SignedInteger,
    UnsignedInteger, String,
    Container, Enum,
    Pointer,
    IStream,
    OStream
    >;
    VariantT var_;
    
    template <class T>
    Variant(const T &_rv):var_{_rv}{}
    
    template <class T>
    Variant& operator=(T &&_rv){
        var_ = std::move(_rv);
        return *this;
    }
    
    
    
    const auto* generic()const{
        return std::get_if<Generic>(&var_);
    }
    
    const auto* signedInteger()const{
        return std::get_if<SignedInteger>(&var_);
    }
    const auto* unsignedInteger()const{
        return std::get_if<UnsignedInteger>(&var_);
    }
    const auto* string()const{
        return std::get_if<String>(&var_);
    }
    const auto* container()const{
        return std::get_if<Container>(&var_);
    }
    const auto* enumeration()const{
        return std::get_if<Enum>(&var_);
    }
    const auto* pointer()const{
        return std::get_if<Pointer>(&var_);
    }
    const auto* istream()const{
        return std::get_if<IStream>(&var_);
    }
    const auto* ostream()const{
        return std::get_if<OStream>(&var_);
    }
};

inline constexpr auto    factory = [](const auto &_rt, const TypeMapBase *_ptype_map) -> auto{
    using value_t = std::decay_t<decltype(_rt)>;
    if constexpr (std::is_enum_v<value_t>){
        return Enum{};
    }else if constexpr (is_shared_ptr_v<value_t> || is_unique_ptr_v<value_t>){
        return Pointer{_ptype_map};
    }else if constexpr (std::is_signed_v<value_t>){
        return SignedInteger{std::numeric_limits<value_t>::min(), std::numeric_limits<value_t>::max()};
    }else if constexpr (std::is_unsigned_v<value_t>){
        return UnsignedInteger{std::numeric_limits<value_t>::max()};
    }else if constexpr (std::is_same_v<value_t, std::string>){
        return String{};
    }else if constexpr (solid::is_container<value_t>::value){
        return Container{};
    }else if constexpr (std::is_base_of_v<std::istream, value_t>){
        return IStream{};
    }else if constexpr (std::is_base_of_v<std::ostream, value_t>){
        return OStream{};
    }else{
        return Generic{};
    }
};


}//namespace metadata
}//namespace v1
}//namespace reflection
}//namespace solid
