// solid/serialization/json/v1/serialization.hpp
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

#include <charconv>
#include <ostream>
#include <string>
#include <string_view>

namespace solid::serialization::json::inline v1 {

namespace reflection = solid::reflection::v2;

namespace detail {
__attribute__((always_inline)) inline void append_int(std::string& out, auto val)
{
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val);
    out.append(buf, ptr);
}
} // namespace detail

// —— JSON serialization ——
// Everything is written through the metadata descriptors' public API only:
// get()/get_sv() for scalars, for_each() for containers (one descriptor per
// element), reflect() for nested reflectables and with_if() for
// optionals/pointers/variants. The walked values are never touched directly.

template <typename Ctx>
class JsonWriter {
    std::string& out;

public:
    explicit JsonWriter(std::string& _out)
        : out{_out}
    {
    }

    // Writes '{...}' from an object whose fields are walked by _apply: the
    // callable invokes the given visitor once per field descriptor.
    template <typename ApplyF>
    void object(ApplyF&& _apply, Ctx& _rc)
    {
        out += '{';
        bool first = true;
        _apply([this, &first](auto&& _field, Ctx& _rctx) {
            using namespace reflection;
            using FieldT = std::decay_t<decltype(_field)>;
            if constexpr (FieldT::type_id == TypeIdE::Callable || FieldT::type_id == TypeIdE::IOStream || FieldT::type_id == TypeIdE::OStream || FieldT::type_id == TypeIdE::IStream) {
                return;
            } else {
                if (!first)
                    out += ',';
                first = false;
                out += '"';
                if (not _field.name.empty()) {
                    out.append(_field.name.data(), _field.name.size());
                } else {
                    detail::append_int(out, _field.id);
                }
                out += "\":";
                value(_field, _rctx);
            }
        });
        out += '}';
    }

    // Writes the JSON value of a single descriptor (no key).
    template <typename Field>
    void value(Field&& _f, Ctx& _rc)
    {
        using namespace reflection;
        using FieldT = std::decay_t<Field>;

        if constexpr (FieldT::type_id == TypeIdE::Boolean) {
            out += _f.get() ? "true" : "false";
        } else if constexpr (FieldT::type_id == TypeIdE::Integral) {
            detail::append_int(out, _f.get());
        } else if constexpr (FieldT::type_id == TypeIdE::String) {
            out += '"';
            out += std::string_view{_f.get()};
            out += '"';
        } else if constexpr (FieldT::type_id == TypeIdE::Enum) {
            out += '"';
            out += _f.get_sv();
            out += '"';
        } else if constexpr (FieldT::type_id == TypeIdE::Reflectable) {
            object([&_f, &_rc](auto&& _rv) { _f.reflect(_rv, _rc); }, _rc);
        } else if constexpr (FieldT::type_id == TypeIdE::Container || FieldT::type_id == TypeIdE::Array || FieldT::type_id == TypeIdE::KeyContainer || FieldT::type_id == TypeIdE::KeyValueContainer) {
            out += '[';
            bool first = true;
            _f.for_each([this, &first](auto&& _e, Ctx& _rctx) {
                if (!first)
                    out += ',';
                first = false;
                value(_e, _rctx);
            },
                _rc);
            out += ']';
        } else if constexpr (FieldT::type_id == TypeIdE::Bitset || FieldT::type_id == TypeIdE::BitVector) {
            // Bits are emitted compactly as 1/0 (elements arrive as Boolean
            // descriptors).
            out += '[';
            bool first = true;
            _f.for_each([this, &first](auto&& _e, Ctx&) {
                if (!first)
                    out += ',';
                first = false;
                out += _e.get() ? '1' : '0';
            },
                _rc);
            out += ']';
        } else if constexpr (FieldT::type_id == TypeIdE::Optional || FieldT::type_id == TypeIdE::Pointer) {
            if (!_f.has_value()) {
                out += "null";
            } else if constexpr (reflection::detail::to_type_id<typename FieldT::element_type>() == TypeIdE::Reflectable) {
                // Struct-like content: with_if walks its fields, we own the braces.
                object([&_f, &_rc](auto&& _rv) { _f.with_if(_rv, _rc); }, _rc);
            } else {
                // Scalar/container content arrives as a single descriptor.
                _f.with_if([this](auto&& _e, Ctx& _rctx) { value(_e, _rctx); }, _rc);
            }
        } else if constexpr (FieldT::type_id == TypeIdE::Variant) {
            out += '[';
            detail::append_int(out, _f.index());
            out += ',';
            if (!_f.has_value()) {
                out += "null";
            } else {
                auto const vidx = _f.index();
                [&]<size_t... Is>(std::index_sequence<Is...>) {
                    auto try_alt = [&]<size_t I>() -> bool {
                        if (vidx != I)
                            return false;
                        using AltT = std::decay_t<std::variant_alternative_t<I, typename FieldT::value_type>>;
                        if constexpr (std::is_same_v<AltT, std::monostate>) {
                            out += "null";
                        } else if constexpr (reflection::detail::to_type_id<AltT>() == TypeIdE::Reflectable) {
                            object([&_f, &_rc](auto&& _rv) { _f.with_if(_rv, _rc); }, _rc);
                        } else {
                            _f.with_if([this](auto&& _e, Ctx& _rctx) { value(_e, _rctx); }, _rc);
                        }
                        return true;
                    };
                    (try_alt.template operator()<Is>() || ...);
                }(std::make_index_sequence<std::variant_size_v<typename FieldT::value_type>>{});
            }
            out += ']';
        }
    }
};

template <typename T, typename Ctx>
void to_json_str(std::string& out, T const& _t, Ctx& _ctx, std::string_view _name = {})
{
    JsonWriter<Ctx> writer{out};
    writer.object([&](auto&& _rv) { reflection::reflect(_t, _rv, _ctx, _name); }, _ctx);
}

template <typename T, typename Ctx>
std::ostream& to_json(std::ostream& _ros, T const& _t, Ctx& _ctx, std::string_view _name = {})
{
    std::string buf;
    to_json_str(buf, _t, _ctx, _name);
    _ros.write(buf.data(), static_cast<std::streamsize>(buf.size()));
    return _ros;
}

} // namespace solid::serialization::json::inline v1
