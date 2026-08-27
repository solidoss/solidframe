// solid/serialization/json/v1/deserialization.hpp
//
// Copyright (c) 2026 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/reflection/v2/reflection.hpp"

#include <expected>
#include <format>
#include <istream>
#include <string>
#include <string_view>

namespace solid::serialization::json::inline v1 {

namespace reflection = solid::reflection::v2;

struct ErrorContext {
    std::string reason;
    std::size_t offset{0};
    std::size_t line{0};
    std::size_t column{0};
    std::string path;

    std::string message() const
    {
        if (path.empty()) {
            return std::format("(line {}, column {}, offset {}): {}",
                line, column, offset, reason);
        }
        return std::format("at '{}' (line {}, column {}, offset {}): {}",
            path, line, column, offset, reason);
    }
};

template <typename T, typename Ctx>
std::expected<void, ErrorContext>
from_json(std::string_view sv, T& _rt, Ctx& _ctx);

namespace from_json_detail {

__attribute__((always_inline)) inline void skip_ws(std::string_view& s)
{
    auto const* p   = s.data();
    auto const* end = p + s.size();
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        ++p;
    s.remove_prefix(static_cast<size_t>(p - s.data()));
}

__attribute__((always_inline)) inline bool consume(std::string_view& s, char c)
{
    if (s.empty() || s.front() != c)
        return false;
    s.remove_prefix(1);
    return true;
}

// Parse a JSON string into an existing std::string (avoids optional overhead).
// Returns true on success.
inline bool parse_string_into(std::string_view& s, std::string& out)
{
    if (s.empty() || s.front() != '"')
        return false;
    auto const* p   = s.data() + 1;
    auto const* end = s.data() + s.size();
    // Fast path: scan for closing quote, bail on backslash
    for (auto const* q = p; q < end; ++q) {
        if (*q == '"') {
            out.assign(p, static_cast<size_t>(q - p));
            s.remove_prefix(static_cast<size_t>(q - s.data() + 1));
            return true;
        }
        if (*q == '\\')
            break;
    }
    // Slow path: has escape sequences
    s.remove_prefix(1);
    out.clear();
    while (!s.empty() && s.front() != '"') {
        if (s.front() == '\\') {
            s.remove_prefix(1);
            if (s.empty())
                return false;
            switch (s.front()) {
            case '"':
                out += '"';
                break;
            case '\\':
                out += '\\';
                break;
            case '/':
                out += '/';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            default:
                out += s.front();
            }
        } else {
            out += s.front();
        }
        s.remove_prefix(1);
    }
    if (s.empty())
        return false;
    s.remove_prefix(1);
    return true;
}

// Zero-allocation key parsing: returns string_view when no escapes.
// Uses raw pointer arithmetic for speed on short keys.
__attribute__((always_inline)) inline std::string_view parse_string_view(std::string_view& s, bool& ok)
{
    auto const* p = s.data();
    if (p == s.data() + s.size() || *p != '"') {
        ok = false;
        return {};
    }
    ++p;
    auto const* start = p;
    auto const* end   = s.data() + s.size();
    while (p < end) {
        auto c = *p;
        if (c == '"') {
            auto result = std::string_view(start, static_cast<size_t>(p - start));
            s.remove_prefix(static_cast<size_t>(p - s.data() + 1));
            ok = true;
            return result;
        }
        if (c == '\\') {
            ok = false;
            return {};
        }
        ++p;
    }
    ok = false;
    return {};
}

__attribute__((always_inline)) inline bool parse_number(std::string_view& s, auto& out)
{
    using ValueT    = std::decay_t<decltype(out)>;
    auto const* p   = s.data();
    auto const* end = p + s.size();
    bool        neg = false;
    if (p < end && *p == '-') {
        neg = true;
        ++p;
    }
    if (p >= end || static_cast<unsigned char>(*p - '0') > 9u) {
        return false;
    }
    ValueT val{};
    while (p < end && static_cast<unsigned char>(*p - '0') <= 9u) {
        val = static_cast<ValueT>(val * 10 + (*p++ - '0'));
    }
    if constexpr (std::is_signed_v<ValueT>) {
        if (neg) {
            val = -val;
        }
    }
    out = val;
    s.remove_prefix(static_cast<size_t>(p - s.data()));
    return true;
}

// Forward declarations for mutual recursion.
template <typename T, typename Ctx>
bool parse_elem(std::string_view& s, T& out, Ctx& rc);

template <typename Ctx, typename ApplyF>
bool parse_object_with(std::string_view& s, Ctx& rc, ApplyF&& apply);

template <typename T, typename Ctx>
bool parse_object(std::string_view& s, T& obj, Ctx& rc);

template <typename Field, typename Ctx>
bool parse_field(std::string_view& s, Field&& _field, Ctx& rc);

// Dispatches on to_type_id<T>() — used for values the parser owns directly:
// container elements are built as locals and handed to the metadata layer via
// emplace/emplace_back, so no reference into the walked object is ever needed.
// Note: to_type_id maps both std::array and regular containers to TypeIdE::Container;
// the Container branch uses StdArrayC to distinguish them at that one point.
template <typename T, typename Ctx>
bool parse_elem(std::string_view& s, T& out, Ctx& rc)
{
    using namespace reflection;
    using ValT                    = std::decay_t<T>;
    constexpr TypeIdE elem_typeid = reflection::detail::to_type_id<ValT>();
    skip_ws(s);

    if constexpr (elem_typeid == TypeIdE::Boolean) {
        if (s.starts_with("true")) {
            out = true;
            s.remove_prefix(4);
            return true;
        }
        if (s.starts_with("false")) {
            out = false;
            s.remove_prefix(5);
            return true;
        }
        return false;
    } else if constexpr (elem_typeid == TypeIdE::Integral) {
        return parse_number(s, out);
    } else if constexpr (elem_typeid == TypeIdE::String) {
        return parse_string_into(s, out);
    } else if constexpr (elem_typeid == TypeIdE::Enum) {
        bool ok   = false;
        auto name = parse_string_view(s, ok);
        if (ok) {
            if (auto val = from_string(std::type_identity<ValT>{}, name)) {
                out = *val;
                return true;
            }
            return false;
        }
        // Slow path: escaped characters in the name (unusual for enums).
        std::string tmp;
        if (!parse_string_into(s, tmp))
            return false;
        if (auto val = from_string(std::type_identity<ValT>{}, std::string_view{tmp})) {
            out = *val;
            return true;
        }
        return false;
    } else if constexpr (elem_typeid == TypeIdE::Bitset) {
        if (!consume(s, '['))
            return false;
        skip_ws(s);
        out.reset();
        if (!s.empty() && s.front() == ']') {
            s.remove_prefix(1);
            return true;
        }
        size_t idx = 0;
        while (true) {
            skip_ws(s);
            if (idx >= out.size())
                return false;
            if (s.starts_with("1")) {
                out[idx++] = true;
                s.remove_prefix(1);
            } else if (s.starts_with("0")) {
                out[idx++] = false;
                s.remove_prefix(1);
            } else
                return false;
            skip_ws(s);
            if (!s.empty() && s.front() == ',') {
                s.remove_prefix(1);
                continue;
            }
            if (!s.empty() && s.front() == ']') {
                s.remove_prefix(1);
                return true;
            }
            return false;
        }
    } else if constexpr (elem_typeid == TypeIdE::BitVector) {
        if (!consume(s, '['))
            return false;
        skip_ws(s);
        out.clear();
        if (!s.empty() && s.front() == ']') {
            s.remove_prefix(1);
            return true;
        }
        while (true) {
            skip_ws(s);
            if (s.starts_with("1")) {
                out.emplace_back(true);
                s.remove_prefix(1);
            } else if (s.starts_with("0")) {
                out.emplace_back(false);
                s.remove_prefix(1);
            } else
                return false;
            skip_ws(s);
            if (!s.empty() && s.front() == ',') {
                s.remove_prefix(1);
                continue;
            }
            if (!s.empty() && s.front() == ']') {
                s.remove_prefix(1);
                return true;
            }
            return false;
        }
    } else if constexpr (elem_typeid == TypeIdE::Container || elem_typeid == TypeIdE::KeyContainer || elem_typeid == TypeIdE::KeyValueContainer) {
        // to_type_id returns Container for both std::array and regular containers.
        if (!consume(s, '['))
            return false;
        skip_ws(s);
        if (!s.empty() && s.front() == ']') {
            s.remove_prefix(1);
            return true;
        }
        if constexpr (reflection::detail::StdArrayC<ValT>) {
            size_t idx = 0;
            while (true) {
                skip_ws(s);
                if (idx >= out.size())
                    return false;
                if (!parse_elem(s, out[idx++], rc))
                    return false;
                skip_ws(s);
                if (!s.empty() && s.front() == ',') {
                    s.remove_prefix(1);
                    continue;
                }
                if (!s.empty() && s.front() == ']') {
                    s.remove_prefix(1);
                    return true;
                }
                return false;
            }
        } else if constexpr (requires { typename ValT::key_type; typename ValT::mapped_type; }) {
            using KeyT    = std::decay_t<typename ValT::key_type>;
            using MappedT = std::decay_t<typename ValT::mapped_type>;
            out.clear();
            while (true) {
                skip_ws(s);
                std::pair<KeyT, MappedT> elem{};
                if (!parse_object(s, elem, rc))
                    return false;
                out.emplace(std::move(elem.first), std::move(elem.second));
                skip_ws(s);
                if (!s.empty() && s.front() == ',') {
                    s.remove_prefix(1);
                    continue;
                }
                if (!s.empty() && s.front() == ']') {
                    s.remove_prefix(1);
                    return true;
                }
                return false;
            }
        } else if constexpr (requires { typename ValT::key_type; }) {
            using ElemT = std::decay_t<typename ValT::value_type>;
            out.clear();
            while (true) {
                skip_ws(s);
                ElemT elem{};
                if (!parse_elem(s, elem, rc))
                    return false;
                out.emplace(std::move(elem));
                skip_ws(s);
                if (!s.empty() && s.front() == ',') {
                    s.remove_prefix(1);
                    continue;
                }
                if (!s.empty() && s.front() == ']') {
                    s.remove_prefix(1);
                    return true;
                }
                return false;
            }
        } else {
            using ElemT = std::decay_t<typename ValT::value_type>;
            out.clear();
            while (true) {
                skip_ws(s);
                ElemT elem{};
                if (!parse_elem(s, elem, rc))
                    return false;
                out.emplace_back(std::move(elem));
                skip_ws(s);
                if (!s.empty() && s.front() == ',') {
                    s.remove_prefix(1);
                    continue;
                }
                if (!s.empty() && s.front() == ']') {
                    s.remove_prefix(1);
                    return true;
                }
                return false;
            }
        }
    } else if constexpr (elem_typeid == TypeIdE::Optional) {
        if (s.starts_with("null")) {
            out.reset();
            s.remove_prefix(4);
            return true;
        }
        out.emplace();
        return parse_elem(s, *out, rc);
    } else if constexpr (elem_typeid == TypeIdE::Pointer) {
        if (s.starts_with("null")) {
            out.reset();
            s.remove_prefix(4);
            return true;
        }
        using ElemT = typename ValT::element_type;
        if constexpr (reflection::detail::is_shared_ptr_v<ValT>)
            out = std::make_shared<ElemT>();
        else
            out = std::make_unique<ElemT>();
        return parse_elem(s, *out, rc);
    } else if constexpr (elem_typeid == TypeIdE::Variant) {
        if (!consume(s, '['))
            return false;
        skip_ws(s);
        size_t vidx = 0;
        if (!parse_number(s, vidx))
            return false;
        skip_ws(s);
        if (!consume(s, ','))
            return false;
        skip_ws(s);
        bool ok = [&]<size_t... Is>(std::index_sequence<Is...>) -> bool {
            auto try_alt = [&]<size_t I>() -> bool {
                if (vidx != I)
                    return false;
                using AltT = std::variant_alternative_t<I, ValT>;
                auto& alt  = out.template emplace<I>();
                if constexpr (std::is_same_v<AltT, std::monostate>) {
                    if (!s.starts_with("null"))
                        return false;
                    s.remove_prefix(4);
                    return true;
                } else {
                    return parse_elem(s, alt, rc);
                }
            };
            return (try_alt.template operator()<Is>() || ...);
        }(std::make_index_sequence<std::variant_size_v<ValT>>{});
        if (!ok)
            return false;
        skip_ws(s);
        return consume(s, ']');
    } else {
        // TypeIdE::Reflectable: struct, tuple, pair
        return parse_object(s, out, rc);
    }
}

// Parses one JSON value into the field behind a metadata descriptor, going
// exclusively through the descriptor's public API: set()/set_sv() for scalars,
// clear()/emplace()/emplace_back() for containers, reflect() for nested
// reflectables and emplace()/make()/with_if() for optionals/pointers/variants.
template <typename Field, typename Ctx>
bool parse_field(std::string_view& s, Field&& _field, Ctx& rc)
{
    using namespace reflection;
    using FieldT = std::decay_t<Field>;
    skip_ws(s);

    if constexpr (FieldT::type_id == TypeIdE::Boolean) {
        if (s.starts_with("true")) {
            _field.set(true);
            s.remove_prefix(4);
            return true;
        }
        if (s.starts_with("false")) {
            _field.set(false);
            s.remove_prefix(5);
            return true;
        }
        return false;
    } else if constexpr (FieldT::type_id == TypeIdE::Integral) {
        typename FieldT::value_type val{};
        if (!parse_number(s, val))
            return false;
        _field.set(val);
        return true;
    } else if constexpr (FieldT::type_id == TypeIdE::String) {
        std::string tmp;
        if (!parse_string_into(s, tmp))
            return false;
        _field.set(std::move(tmp));
        return true;
    } else if constexpr (FieldT::type_id == TypeIdE::Enum) {
        bool ok = false;
        if (auto name = parse_string_view(s, ok); ok)
            return _field.set_sv(name);
        // Slow path: escaped characters in the name (unusual for enums).
        std::string tmp;
        if (!parse_string_into(s, tmp))
            return false;
        return _field.set_sv(std::string_view{tmp});
    } else if constexpr (FieldT::type_id == TypeIdE::Reflectable) {
        return parse_object_with(s, rc, [&](auto&& _rv) { _field.reflect(_rv, rc); });
    } else if constexpr (FieldT::type_id == TypeIdE::Bitset || FieldT::type_id == TypeIdE::BitVector) {
        if (!consume(s, '['))
            return false;
        _field.clear();
        skip_ws(s);
        if (!s.empty() && s.front() == ']') {
            s.remove_prefix(1);
            return true;
        }
        while (true) {
            skip_ws(s);
            if (s.starts_with("1")) {
                _field.emplace_back(true);
            } else if (s.starts_with("0")) {
                _field.emplace_back(false);
            } else
                return false;
            s.remove_prefix(1);
            skip_ws(s);
            if (consume(s, ','))
                continue;
            return consume(s, ']');
        }
    } else if constexpr (FieldT::type_id == TypeIdE::Container || FieldT::type_id == TypeIdE::Array || FieldT::type_id == TypeIdE::KeyContainer || FieldT::type_id == TypeIdE::KeyValueContainer) {
        using ValT = std::decay_t<typename FieldT::value_type>;
        if (!consume(s, '['))
            return false;
        _field.clear();
        skip_ws(s);
        if (!s.empty() && s.front() == ']') {
            s.remove_prefix(1);
            return true;
        }
        while (true) {
            skip_ws(s);
            if constexpr (requires { typename ValT::key_type; typename ValT::mapped_type; }) {
                std::pair<std::decay_t<typename ValT::key_type>, std::decay_t<typename ValT::mapped_type>> elem{};
                if (!parse_object(s, elem, rc))
                    return false;
                _field.emplace(std::move(elem.first), std::move(elem.second));
            } else if constexpr (requires { typename ValT::key_type; }) {
                std::decay_t<typename FieldT::element_type> elem{};
                if (!parse_elem(s, elem, rc))
                    return false;
                _field.emplace(std::move(elem));
            } else if constexpr (requires { _field.grow_back(); } && reflection::detail::to_type_id<std::decay_t<typename FieldT::element_type>>() == reflection::TypeIdE::Reflectable) {
                // In-place growth (overlay containers): the next element lives
                // in an underlying buffer and may own trailing variable-size
                // content that a stack-local copy could not hold — parse
                // directly into it.
                if (!parse_object(s, _field.grow_back(), rc))
                    return false;
            } else {
                std::decay_t<typename FieldT::element_type> elem{};
                if (!parse_elem(s, elem, rc))
                    return false;
                _field.emplace_back(std::move(elem));
            }
            skip_ws(s);
            if (consume(s, ','))
                continue;
            return consume(s, ']');
        }
    } else if constexpr (FieldT::type_id == TypeIdE::Optional || FieldT::type_id == TypeIdE::Pointer) {
        if (s.starts_with("null")) {
            _field.reset();
            s.remove_prefix(4);
            return true;
        }
        using ElemT = typename FieldT::element_type;
        if constexpr (FieldT::type_id == TypeIdE::Optional) {
            _field.emplace();
        } else {
            _field.template make<ElemT>();
        }
        if constexpr (reflection::detail::to_type_id<std::decay_t<ElemT>>() == TypeIdE::Reflectable) {
            // Struct-like content: with_if walks its fields for the key matcher.
            return parse_object_with(s, rc, [&](auto&& _rv) { _field.with_if(_rv, rc); });
        } else {
            // Scalar/container content arrives as a single descriptor.
            bool ok = false;
            _field.with_if([&](auto&& _d, Ctx& _rctx) { ok = parse_field(s, _d, _rctx); }, rc);
            return ok;
        }
    } else if constexpr (FieldT::type_id == TypeIdE::Variant) {
        if (!consume(s, '['))
            return false;
        skip_ws(s);
        size_t vidx = 0;
        if (!parse_number(s, vidx))
            return false;
        skip_ws(s);
        if (!consume(s, ','))
            return false;
        skip_ws(s);
        using VarT = typename FieldT::value_type;
        bool ok    = [&]<size_t... Is>(std::index_sequence<Is...>) -> bool {
            auto try_alt = [&]<size_t I>() -> bool {
                if (vidx != I)
                    return false;
                using AltT = std::variant_alternative_t<I, VarT>;
                _field.template emplace<I>();
                if constexpr (std::is_same_v<AltT, std::monostate>) {
                    if (!s.starts_with("null"))
                        return false;
                    s.remove_prefix(4);
                    return true;
                } else if constexpr (reflection::detail::to_type_id<std::decay_t<AltT>>() == TypeIdE::Reflectable) {
                    return parse_object_with(s, rc, [&](auto&& _rv) { _field.with_if(_rv, rc); });
                } else {
                    bool alt_ok = false;
                    _field.with_if([&](auto&& _d, Ctx& _rctx) { alt_ok = parse_field(s, _d, _rctx); }, rc);
                    return alt_ok;
                }
            };
            return (try_alt.template operator()<Is>() || ...);
        }(std::make_index_sequence<std::variant_size_v<VarT>>{});
        if (!ok)
            return false;
        skip_ws(s);
        return consume(s, ']');
    } else {
        // Callable / stream fields carry no JSON value.
        return false;
    }
}

// Parses '{...}' by repeatedly re-walking the fields via apply(visitor) and
// letting the matching field descriptor consume its value through parse_field.
template <typename Ctx, typename ApplyF>
bool parse_object_with(std::string_view& s, Ctx& rc, ApplyF&& apply)
{
    skip_ws(s);
    if (!consume(s, '{'))
        return false;
    skip_ws(s);
    if (!s.empty() && s.front() == '}') {
        s.remove_prefix(1);
        return true;
    }

    std::string key_owned; // hoisted: reuse buffer across iterations

    while (true) {
        skip_ws(s);
        bool             sv_ok = false;
        std::string_view key   = parse_string_view(s, sv_ok);
        if (!sv_ok) {
            if (!parse_string_into(s, key_owned))
                return false;
            key = key_owned;
        }
        skip_ws(s);
        if (!consume(s, ':'))
            return false;

        bool field_found = false;
        bool field_ok    = true;

        apply([&](auto&& _field, Ctx& _rc) {
            using FieldT = std::decay_t<decltype(_field)>;
            if constexpr (FieldT::type_id == reflection::TypeIdE::Callable || FieldT::type_id == reflection::TypeIdE::IOStream || FieldT::type_id == reflection::TypeIdE::OStream || FieldT::type_id == reflection::TypeIdE::IStream) {
                return;
            } else {
                if (field_found)
                    return;
                if (_field.name.empty()) {
                    size_t           idx = 0;
                    std::string_view num{key};
                    if (!parse_number(num, idx) || !num.empty() || idx != _field.id)
                        return;
                } else if (_field.name != key) {
                    return;
                }
                field_found = true;
                field_ok    = parse_field(s, _field, _rc);
            }
        });

        if (!field_found || !field_ok)
            return false;
        skip_ws(s);
        if (consume(s, ','))
            continue;
        return consume(s, '}');
    }
}

template <typename T, typename Ctx>
bool parse_object(std::string_view& s, T& obj, Ctx& rc)
{
    return parse_object_with(s, rc, [&](auto&& _rv) { reflection::reflect(obj, _rv, rc); });
}

} // namespace from_json_detail

template <typename T, typename Ctx>
std::expected<void, ErrorContext>
from_json(std::string_view sv, T& _rt, Ctx& _ctx)
{
    auto const original = sv;
    if (!from_json_detail::parse_object(sv, _rt, _ctx)) {
        ErrorContext err;
        err.reason       = "JSON parse error";
        err.offset       = original.size() - sv.size();
        std::size_t line = 1, col = 1;
        for (std::size_t i = 0; i < err.offset && i < original.size(); ++i) {
            if (original[i] == '\n') {
                ++line;
                col = 1;
            } else {
                ++col;
            }
        }
        err.line   = line;
        err.column = col;
        return std::unexpected(std::move(err));
    }
    return {};
}

// Reads the stream to EOF into a contiguous buffer, then parses it with the
// string_view overload — one pass over bulk reads beats per-token stream
// extraction by a wide margin. Error offsets/lines refer to the buffered text.
template <typename T, typename Ctx>
std::expected<void, ErrorContext>
from_json(std::istream& _ris, T& _rt, Ctx& _ctx)
{
    std::string buf;
    if (auto* psb = _ris.rdbuf(); psb != nullptr) {
        if (auto const avail = psb->in_avail(); avail > 0) {
            buf.reserve(static_cast<size_t>(avail));
        }
    }
    char chunk[64 * 1024];
    while (_ris.read(chunk, sizeof(chunk)) || _ris.gcount() > 0) {
        buf.append(chunk, static_cast<size_t>(_ris.gcount()));
    }
    return from_json(std::string_view{buf}, _rt, _ctx);
}

} // namespace solid::serialization::json::inline v1
