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

std::ostream& Event::print(std::ostream& _ros) const
{
    return _ros << pcategory_->name() << ':' << ':' << pcategory_->name(*this);
}

//-----------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& _ros, Event const& _re)
{
    return _re.print(_ros);
}

//-----------------------------------------------------------------------------

const Event generic_event_default(make_event(GenericEvents::Default));
const Event generic_event_start(make_event(GenericEvents::Start));
const Event generic_event_stop(make_event(GenericEvents::Stop));
const Event generic_event_raise(make_event(GenericEvents::Raise));
const Event generic_event_message(make_event(GenericEvents::Message));
const Event generic_event_timer(make_event(GenericEvents::Timer));
const Event generic_event_pause(make_event(GenericEvents::Pause));
const Event generic_event_resume(make_event(GenericEvents::Resume));
const Event generic_event_update(make_event(GenericEvents::Update));
const Event generic_event_kill(make_event(GenericEvents::Kill));

} // namespace solid
