// solid/reflection/v1/ostreamreflection.hpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/reflection/v1/reflector.hpp"
#include <ostream>

namespace solid {
namespace reflection {
namespace v1 {

namespace impl {
struct OStreamVisitor {
    bool                   do_indent_    = true;
    size_t                 indent_count_ = 0;
    std::ostream&          rostream_;
    const std::string_view indent_;
    const std::string_view eol_;
    OStreamVisitor(
        std::ostream&           _rostream,
        const std::string_view& _indent, const std::string_view& _eol)
        : rostream_(_rostream)
        , indent_(_indent)
        , eol_(_eol)
    {
    }

    void indent()
    {
        for (size_t i = 0; i < indent_count_; ++i) {
            rostream_ << indent_;
        }
    }

    template <class Node, class Meta, class Ctx>
    void operator()(Node& _rnode, const Meta& _rmeta, const size_t _index, const char* const _name, Ctx& _rctx)
    {
        using tg = TypeGroupE;
        if (do_indent_) {
            indent();
        } else {
            do_indent_ = true;
        }
        switch (_rnode.typeGroup()) {
        case tg::Basic: {
            auto& rbasic_node = *_rnode.template as<tg::Basic>();
            rostream_ << _name << '(' << _index << ") = ";
            rbasic_node.print(rostream_);
            rostream_ << eol_;
        } break;
        case tg::Bitset: {
            auto& rbasic_node = *_rnode.template as<tg::Bitset>();
            rostream_ << _name << '(' << _index << ") = ";
            rbasic_node.print(rostream_);
            rostream_ << eol_;
        } break;
        case tg::Enum: {
            auto& renum_node = *_rnode.template as<tg::Enum>();
            rostream_ << _name << '(' << _index << ") = ";
            renum_node.print(rostream_);
            rostream_ << eol_;
        } break;
        case tg::Structure:
            rostream_ << _name << '(' << _index << ") = {" << eol_;
            ++indent_count_;
            _rnode.template as<tg::Structure>()->for_each(std::ref(*this), _rctx);
            --indent_count_;
            indent();
            rostream_ << "}" << eol_;
            break;
        case tg::Container:
            rostream_ << _name << '(' << _index << ") = [" << eol_;
            ++indent_count_;
            _rnode.template as<tg::Container>()->for_each(std::ref(*this), _rctx);
            --indent_count_;
            indent();
            rostream_ << "]" << eol_;
            break;
        case tg::Array:
            rostream_ << _name << '(' << _index << ") = [" << eol_;
            ++indent_count_;
            _rnode.template as<tg::Array>()->for_each(std::ref(*this), _rctx);
            --indent_count_;
            indent();
            rostream_ << "]" << eol_;
            break;
        case tg::IStream:
            rostream_ << _name << '(' << _index << ") = <istream>" << eol_;
            break;
        case tg::OStream:
            rostream_ << _name << '(' << _index << ") = <ostream>" << eol_;
            break;
        case tg::Tuple:
            rostream_ << _name << '(' << _index << ") = {" << eol_;
            ++indent_count_;
            _rnode.template as<tg::Tuple>()->for_each(std::ref(*this), _rctx);
            --indent_count_;
            indent();
            rostream_ << "}" << eol_;
            break;
        case tg::SharedPtr: {
            rostream_ << _name << '(' << _index << ") -> ";
            auto& rptr_node = *_rnode.template as<tg::SharedPtr>();
            if (!rptr_node.isNull()) {
                do_indent_ = false;
                rptr_node.visit(std::ref(*this), _rmeta.pointer()->map(), _rctx);
            } else {
                rostream_ << "nullptr" << eol_;
            }
        } break;
        case tg::UniquePtr: {
            rostream_ << _name << '(' << _index << ") -> ";
            auto& rptr_node = *_rnode.template as<tg::UniquePtr>();

            if (!rptr_node.isNull()) {
                do_indent_ = false;
                rptr_node.visit(std::ref(*this), _rmeta.pointer()->map(), _rctx);
            } else {
                rostream_ << "nullptr" << eol_;
            }
        } break;
        case tg::IntrusivePtr: {
            rostream_ << _name << '(' << _index << ") -> ";
            auto& rptr_node = *_rnode.template as<tg::IntrusivePtr>();

            if (!rptr_node.isNull()) {
                do_indent_ = false;
                rptr_node.visit(std::ref(*this), _rmeta.pointer()->map(), _rctx);
            } else {
                rostream_ << "nullptr" << eol_;
            }
        } break;
        default:
            break;
        }
    }
};

template <class MetadataVariant, class MetadataFactory, class T, class Context>
struct OReflectStub {
    MetadataFactory&&        meta_data_factory_;
    const T&                 rt_;
    Context&                 rctx_;
    const TypeMapBase* const ptype_map_ = nullptr;
    const std::string_view   indent_;
    const std::string_view   eol_;

    explicit OReflectStub(
        MetadataFactory&& _meta_data_factory, const T& _rt,
        Context& _rctx, const TypeMapBase* const _ptype_map,
        const std::string_view& _indent, const std::string_view& _eol)
        : meta_data_factory_(std::forward<MetadataFactory>(_meta_data_factory))
        , rt_(_rt)
        , rctx_(_rctx)
        , ptype_map_(_ptype_map)
        , indent_(_indent)
        , eol_(_eol)
    {
    }

    void print(std::ostream& _ros) const
    {
        impl::OStreamVisitor visitor{_ros, indent_, eol_};

        const_reflect<MetadataVariant>(meta_data_factory_, rt_, std::ref(visitor), rctx_, ptype_map_);
    }
};

} // namespace impl

template <class MetadataVariant, class MetadataFactory, class T, class Context = solid::EmptyType>
auto oreflect(
    MetadataFactory&& _meta_data_factory, const T& _rt, Context& _rctx, const TypeMapBase* const _ptype_map = nullptr,
    const std::string_view& _indent = "  ", const std::string_view& _eol = "\r\n")
{
    return impl::OReflectStub<MetadataVariant, MetadataFactory, T, Context>{_meta_data_factory, _rt, _rctx, _ptype_map, _indent, _eol};
}
} // namespace v1
} // namespace reflection
} // namespace solid

template <class MetadataVariant, class MetadataFactory, class T, class Context>
std::ostream& operator<<(std::ostream& _ros, const solid::reflection::v1::impl::OReflectStub<MetadataVariant, MetadataFactory, T, Context>& _rstub)
{
    _rstub.print(_ros);
    return _ros;
}
