// solid/serialization/v2/serialization.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/serialization/v2/binarydeserializer.hpp"
#include "solid/serialization/v2/binaryserializer.hpp"
#include "solid/serialization/v2/typemap.hpp"
namespace solid {
namespace serialization {
using namespace v2;

template <class T, class S>
void addVersion(S& _rs, const uint32_t& _version, const char* _name)
{
    _rs.template addVersion<T>(_version, _name);
}

template <class T, class S>
void addVersion(S& _rs, uint32_t& _rversion, const char* _name)
{
    _rs.template addVersion<T>(_rversion, _name);
}

template <class T, class S>
uint32_t version(S& _rs, const T& _rt)
{
    return _rs.version(_rt);
}

} // namespace serialization
} // namespace solid
