// solid/reflection/v2/reflection_metadata.hpp
//
// Copyright (c) 2026 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#pragma once

#include "solid/reflection/v2/reflection_concept.hpp"

#include <algorithm>
#include <cassert>
#include <functional>
#include <limits>
#include <string_view>
#include <utility>

namespace solid::reflection::v2 {

template <size_t Id, typename Val, typename... Args>
auto make(std::string_view _name, Val& _ref, Args&&... _args);
template <size_t Id, typename GetF, typename SetF, typename... ArgsF>
auto makef(std::string_view _name, GetF&& _get, SetF&& _set, ArgsF&&... _args);

template <size_t Id, typename ValT>
class Boolean {
    ValT& ref;

public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::Boolean};
    using value_type = std::decay_t<ValT>;

    std::string_view const name;

    bool get() const
    {
        return ref;
    }

    void set(bool value)
    {
        ref = value;
    }

    Boolean(std::string_view _name, ValT& _ref)
        : ref{_ref}
        , name{_name}
    {
    }
};

template <size_t Id, typename ValT>
class Integral {
    ValT& ref;

public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::Integral};
    using value_type = std::decay_t<ValT>;

    std::string_view const name;
    value_type             min{std::numeric_limits<value_type>::min()};
    value_type             max{std::numeric_limits<value_type>::max()};

    Integral(std::string_view _name, ValT& _ref)
        : ref{_ref}
        , name{_name}
    {
    }

    value_type get() const
    {
        return ref;
    }

    void set(value_type value)
    {
        ref = value;
    }

    auto& set_min(value_type _val)
    {
        min = _val;
        return *this;
    }
    auto& set_max(value_type _val)
    {
        max = _val;
        return *this;
    }
};

template <size_t Id, typename ValT>
class String {
    ValT& ref;

public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::String};
    using value_type = std::decay_t<ValT>;

    std::string_view const name;
    size_t                 max_size{0};

    String(std::string_view _name, ValT& _ref)
        : ref{_ref}
        , name{_name}
    {
    }

    std::string_view get() const
    {
        return std::string_view{ref};
    }

    void set(std::string_view _sv)
    {
        ref = _sv;
    }

    // Move-in overload used by parsers to hand over an already-built string.
    // Exact-match constrained so const char* / string_view arguments keep
    // binding to the string_view overload above without ambiguity.
    template <typename S>
        requires std::same_as<std::remove_cvref_t<S>, value_type>
    void set(S&& _s)
    {
        ref = std::forward<S>(_s);
    }

    void append(std::string_view _sv)
    {
        ref.append(_sv);
    }

    auto& set_max_size(size_t _size)
    {
        max_size = _size;
        return *this;
    }
};

template <size_t Id, typename ValT>
class Reflectable {
    ValT& ref;

public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::Reflectable};
    using value_type = std::decay_t<ValT>;

    std::string_view const name;

    Reflectable(std::string_view _name, ValT& _ref)
        : ref{_ref}
        , name{_name}
    {
    }

    template <typename T, typename V, typename C>
    void reflect(this T& rthis, V&& _rv, C& _rc);
};

template <size_t Id, typename ValT>
class Container;

template <size_t Id, detail::AnyContainerC ValT>
class Container<Id, ValT> {
    ValT& ref;

public:
    using value_type   = std::decay_t<ValT>;
    using element_type = typename ValT::value_type;
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{detail::to_type_id<ValT>()};

    std::string_view const name;
    size_t                 max_size{0};

    Container(std::string_view _name, ValT& _ref)
        : ref{_ref}
        , name{_name}
    {
    }

    auto& with_max_size(size_t _size)
    {
        max_size = _size;
        return *this;
    }

    template <typename T, typename V, typename C>
    void for_each(this T& rthis, V&& _rv, C& _rc);

    void clear()
    {
        ref.clear();
    }

    auto size() const
    {
        return ref.size();
    }

    template <typename... Args>
    void emplace_back(Args&&... args)
        requires detail::ContainerC<ValT>
    {
        ref.emplace_back(std::forward<Args>(args)...);
    }

    template <typename T, typename V, typename C, typename... Args>
    void emplace_back_with(this T& rthis, V&& _rv, C& _rc, Args&&... args)
        requires detail::ContainerC<ValT>;

    template <typename... Args>
    void emplace(Args&&... args)
        requires detail::KeyContainerC<ValT> || detail::KeyValueContainerC<ValT>
    {
        ref.emplace(std::forward<Args>(args)...);
    }

    template <typename T, typename V, typename C, typename... Args>
    void emplace_with(this T& rthis, V&& _rv, C& _rc, Args&&... args)
        requires detail::KeyContainerC<ValT> || detail::KeyValueContainerC<ValT>
    {
        auto [it, _] = rthis.ref.emplace(std::forward<Args>(args)...);
        std::invoke(std::forward<V>(_rv), *it, _rc);
    }
};

template <size_t Id, detail::StdArrayC ValT>
class Container<Id, ValT> {
    ValT& ref;

public:
    using value_type   = std::decay_t<ValT>;
    using element_type = typename ValT::value_type;
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::Array};

    std::string_view const name;
    size_t                 max_size{std::numeric_limits<size_t>::max()};
    size_t                 offset{0};

    Container(std::string_view _name, ValT& _ref)
        : ref{_ref}
        , name{_name}
    {
    }

    auto& with_max_size(size_t _size)
    {
        max_size = _size;
        return *this;
    }

    auto size() const
    {
        return ref.size();
    }

    template <typename T, typename V, typename C>
    void for_each(this T& rthis, V&& _rv, C& _rc);

    void clear()
    {
        offset = 0;
    }

    template <typename... Args>
    void emplace_back(Args&&... args)
    {
        ref.at(offset++) = {std::forward<Args>(args)...};
    }

    template <typename T, typename V, typename C, typename... Args>
    void emplace_back_with(this T& rthis, V&& _rv, C& _rc, Args&&... args);
};

template <size_t Id, detail::BitsetC ValT>
class Container<Id, ValT> {
    ValT& ref;

public:
    using value_type   = std::decay_t<ValT>;
    using element_type = bool;
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::Bitset};
    static constexpr TypeIdE element_type_id{TypeIdE::Integral};

    std::string_view const name;
    size_t                 max_size;
    size_t                 offset{0};

    Container(std::string_view _name, ValT& _ref)
        : ref{_ref}
        , name{_name}
        , max_size{_ref.size()}
    {
    }

    auto& with_max_size(size_t _size)
    {
        max_size = std::min(_size, ref.size());
        return *this;
    }

    template <typename T, typename V, typename C>
    void for_each(this T& rthis, V&& _rv, C& _rc);

    void clear()
    {
        ref.reset();
    }

    void emplace_back(bool const _bit)
    {
        ref[offset++] = _bit;
    }
};

template <size_t Id, detail::BitVectorC ValT>
class Container<Id, ValT> {
    ValT& ref;

public:
    using value_type   = std::decay_t<ValT>;
    using element_type = bool;
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::BitVector};
    static constexpr TypeIdE element_type_id{TypeIdE::Integral};

    std::string_view const name;
    size_t                 max_size{std::numeric_limits<size_t>::max()};
    size_t                 offset{0};

    Container(std::string_view _name, ValT& _ref)
        : ref{_ref}
        , name{_name}
    {
    }

    auto& with_max_size(size_t _size)
    {
        max_size = _size;
        return *this;
    }

    template <typename T, typename V, typename C>
    void for_each(this T& rthis, V&& _rv, C& _rc);

    void clear()
    {
        ref.clear();
    }

    void emplace_back(bool const _bit)
    {
        ref.emplace_back(_bit);
    }
};

template <size_t Id, typename ValT>
class Callable {
    ValT&& ref;

public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::Callable};

    std::string_view const name;

    Callable(std::string_view _name, ValT&& _ref)
        : ref{std::forward<ValT>(_ref)}
        , name{_name}
    {
    }

    void call(auto&& _rr, auto& _ctx)
    {
        std::invoke(ref, _rr, _ctx);
    }
};

template <size_t Id, typename ProgressF = void>
class IStream;

template <size_t Id>
class IStream<Id, void> {
    std::istream& ref;

public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::IStream};

    std::string_view const name;

    IStream(std::string_view _name, std::istream& _ref)
        : ref{_ref}
        , name{_name}
    {
    }

    std::istream& stream() const
    {
        return ref;
    }

    template <typename ProgressF>
    auto set_progress(ProgressF&& _fnc)
    {
        using ProgressT = std::decay_t<ProgressF>;
        return IStream<Id, ProgressT>{*this, std::forward<ProgressF>(_fnc)};
    }
};

template <size_t Id, typename ProgressF>
class IStream : public IStream<Id, void> {
public:
    ProgressF progress;
};

template <size_t Id, typename ProgressF = void>
class OStream;

template <size_t Id>
class OStream<Id, void> {
    std::ostream& ref;

public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::OStream};

    std::string_view const name;

    OStream(std::string_view _name, std::ostream& _ref)
        : ref{_ref}
        , name{_name}
    {
    }

    std::ostream& stream() const
    {
        return ref;
    }

    template <typename ProgressF>
    auto set_progress(ProgressF&& _fnc)
    {
        using ProgressT = std::decay_t<ProgressF>;
        return IStream<Id, ProgressT>{*this, std::forward<ProgressF>(_fnc)};
    }
};

template <size_t Id, typename ProgressF>
class OStream : public OStream<Id, void> {
public:
    ProgressF progress;
};

template <size_t Id, typename ProgressF = void>
class IOStream;

template <size_t Id>
class IOStream<Id, void> {
    std::iostream& ref;

public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::IOStream};

    std::string_view const name;

    IOStream(std::string_view _name, std::iostream& _ref)
        : ref{_ref}
        , name{_name}
    {
    }

    std::iostream& stream() const
    {
        return ref;
    }

    template <typename ProgressF>
    auto set_progress(ProgressF&& _fnc)
    {
        using ProgressT = std::decay_t<ProgressF>;
        return IStream<Id, ProgressT>{*this, std::forward<ProgressF>(_fnc)};
    }
};

template <size_t Id, typename ProgressF>
class IOStream : public IOStream<Id, void> {
public:
    ProgressF progress;
};

template <size_t Id, typename ValT>
class Pointer {
    ValT& ref;

public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::Pointer};
    using value_type   = std::decay_t<ValT>;
    using element_type = typename ValT::element_type;

    std::string_view const name;

    Pointer(std::string_view _name, ValT& _ref)
        : ref{_ref}
        , name{_name}
    {
    }

    bool has_value() const
    {
        return static_cast<bool>(ref);
    }

    auto& value()
    {
        return *ref;
    }

    template <typename T, typename V, typename C>
    bool with_if(this T& rthis, V&& _rv, C& _rc);

    void reset()
    {
        ref.reset();
    }

    template <typename T, typename... Args>
        requires std::is_same_v<element_type, T> or std::is_base_of_v<element_type, T>
    void make(Args&&... args) const
    {
        if constexpr (detail::is_shared_ptr_v<value_type>) {
            ref = std::make_shared<T>(std::forward<Args>(args)...);
        } else if constexpr (detail::is_unique_ptr_v<value_type>) {
            ref = std::make_unique<T>(std::forward<Args>(args)...);
        }
    }
};

template <size_t Id, typename ValT>
class Optional {
    ValT& ref;

public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::Optional};
    using value_type   = std::decay_t<ValT>;
    using element_type = typename ValT::value_type;

    std::string_view const name;

    Optional(std::string_view _name, ValT& _ref)
        : ref{_ref}
        , name{_name}
    {
    }

    bool has_value() const
    {
        return ref.has_value();
    }

    template <typename T, typename V, typename C>
    bool with_if(this T& rthis, V&& _rv, C& _rc);

    void reset()
    {
        ref.reset();
    }

    template <typename... Args>
    void emplace(Args&&... args) const
    {
        ref.emplace(std::forward<Args>(args)...);
    }
};

template <size_t Id, typename ValT>
class Variant {
    ValT& ref;

public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::Variant};
    using value_type = std::decay_t<ValT>;

    std::string_view const name;

    Variant(std::string_view _name, ValT& _ref)
        : ref{_ref}
        , name{_name}
    {
    }

    constexpr auto index() const
    {
        return ref.index();
    }

    bool has_value() const
    {
        return not std::holds_alternative<std::monostate>(ref);
    }

    template <typename T, typename... Args>
    void emplace(Args&&... args)
    {
        ref.template emplace<T>(std::forward<Args>(args)...);
    }

    template <size_t I, typename... Args>
    void emplace(Args&&... args)
    {
        ref.template emplace<I>(std::forward<Args>(args)...);
    }

    template <typename This, typename T, typename V, typename C, typename... Args>
    void emplace_with(this This& rthis, V&& _rv, C& _rc, Args&&... args);

    template <typename This, size_t I, typename V, typename C, typename... Args>
    void emplace_with(this This& rthis, V&& _rv, C& _rc, Args&&... args);

    template <typename This, typename V, typename C>
    bool with_if(this This& rthis, V&& _rv, C& _rc);
};

// Meta descriptor for enum fields. Serialized to/from its string name via the
// ADL-reachable to_string / from_string customization points (see NamedEnumC).
// get() yields the current value's name; set() parses a name back into the enum
// and reports whether the name was recognized.
template <size_t Id, typename ValT, typename GetF, typename SetF>
class Enum {
    ValT& ref;

public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::Enum};
    using value_type      = std::decay_t<ValT>;
    using underlying_type = std::underlying_type_t<value_type>;

    std::string_view const name;
    GetF                   get_sv;
    SetF                   set_sv;

    value_type get() const
    {
        return ref;
    }

    void set(value_type value)
    {
        ref = value;
    }

    Enum(std::string_view _name, ValT& _ref, GetF _get, SetF _set)
        : ref{_ref}
        , name{_name}
        , get_sv{_get}
        , set_sv{_set}
    {
    }
};

// —— Function-based descriptors ——
// Mirror the public surface (id, type_id, name, get/set/view, for_each, ...)
// of their reference-based counterparts so serializers dispatch identically,
// but hold no reference to the reflected member: all access goes through the
// get/set closures. Needed for types whose state is only reachable via
// accessor methods (see makef and SFR_V2_MAKEF).

template <size_t Id, typename ValT, typename GetF, typename SetF>
class BooleanF {
public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::Boolean};
    using value_type = ValT;

    std::string_view const name;
    GetF                   get;
    SetF                   set;

    BooleanF(std::string_view _name, GetF _get, SetF _set)
        : name{_name}
        , get{_get}
        , set{_set}
    {
    }
};

template <size_t Id, typename ValT, typename GetF, typename SetF>
class IntegralF {
public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::Integral};
    using value_type = ValT;

    std::string_view const name;
    GetF                   get;
    SetF                   set;
    value_type             min{std::numeric_limits<value_type>::min()};
    value_type             max{std::numeric_limits<value_type>::max()};

    IntegralF(std::string_view _name, GetF _get, SetF _set)
        : name{_name}
        , get{_get}
        , set{_set}
    {
    }

    auto& with_min(value_type _val)
    {
        min = _val;
        return *this;
    }
    auto& with_max(value_type _val)
    {
        max = _val;
        return *this;
    }
};

template <size_t Id, typename ValT, typename GetF, typename SetF, typename ViewF>
class StringF {
public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::String};
    using value_type = ValT;

    std::string_view const name;
    GetF                   get;
    SetF                   set;
    ViewF                  view;
    size_t                 max_size{0};

    StringF(std::string_view _name, GetF _get, SetF _set, ViewF _view)
        : name{_name}
        , get{_get}
        , set{_set}
        , view{_view}
    {
    }

    void append(std::string_view _sv)
    {
        set(get() + _sv);
    }

    auto& with_max_size(size_t _size)
    {
        max_size = _size;
        return *this;
    }
};

template <size_t Id, typename ValT, typename GetF, typename SetF, typename GetSvF, typename SetSvF>
class EnumF {
public:
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::Enum};
    using value_type      = ValT;
    using underlying_type = std::underlying_type_t<value_type>;

    std::string_view const name;
    GetF                   get;
    SetF                   set;
    GetSvF                 get_sv;
    SetSvF                 set_sv;

    EnumF(std::string_view _name, GetF _get, SetF _set, GetSvF _get_sv, SetSvF _set_sv)
        : name{_name}
        , get{_get}
        , set{_set}
        , get_sv(_get_sv)
        , set_sv(_set_sv)
    {
    }
};

// Holds the container/view returned by the getter *by value* — for accessor
// APIs this is typically a std::span, i.e. a cheap view whose mutations reach
// the underlying storage. Writing goes through clear()/emplace_back() with a
// running offset (fixed-capacity semantics, like the std::array Container).
// for_each honors max_size, which enables repeating groups: a fixed wire-size
// array of which only the first N (e.g. max_depth) entries are active —
// .with_max_size(N) limits serialization to those.
template <size_t Id, typename ContT, typename GetF, typename SetF>
class ContainerF {
    ContT cont;

public:
    using value_type   = std::decay_t<ContT>;
    using element_type = typename value_type::value_type;
    static constexpr size_t  id{Id};
    static constexpr TypeIdE type_id{TypeIdE::Container};

    std::string_view const name;
    GetF                   get;
    SetF                   set;
    size_t                 max_size{std::numeric_limits<size_t>::max()};
    size_t                 offset{0};

    ContainerF(std::string_view _name, GetF _get, SetF _set)
        : cont{_get()}
        , name{_name}
        , get{_get}
        , set{_set}
    {
    }

    auto& with_max_size(size_t _size)
    {
        max_size = _size;
        return *this;
    }

    template <typename V, typename C>
    void for_each(V&& _rv, C& _rc)
    {
        size_t count{0};
        for (auto& re : cont) {
            if (count++ >= max_size)
                break;
            std::invoke(_rv, make<0>(""sv, re), _rc);
        }
    }

    auto size() const
    {
        return max_size;
    }

    void clear()
    {
        offset = 0;
    }

    template <typename... Args>
    void emplace_back(Args&&... args)
    {
        assert(offset < cont.size());
        cont[offset++] = {std::forward<Args>(args)...};
    }
};

} // namespace solid::reflection::v2
