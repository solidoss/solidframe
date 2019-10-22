// solid/utility/src/any.cpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/utility/any.hpp"
#include "solid/utility/function.hpp"

namespace solid {

namespace impl {

/*virtual*/ FunctionValueBase::~FunctionValueBase()
{
}

/*virtual*/ AnyValueBase::~AnyValueBase()
{
}

} //namespace impl

} //namespace solid
