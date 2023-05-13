// solid/frame/src/event.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/utility/event.hpp"
#include "solid/system/cassert.hpp"

namespace solid {

//-----------------------------------------------------------------------------

std::ostream& EventBase::print(std::ostream& _ros) const
{
    return _ros << pcategory_->name() << ':' << ':' << pcategory_->eventName(*this);
}

//-----------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& _ros, EventBase const& _re)
{
    return _re.print(_ros);
}

} // namespace solid
