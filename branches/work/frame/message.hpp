// frame/message.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_SIGNAL_HPP
#define SOLID_FRAME_SIGNAL_HPP

#include "frame/common.hpp"

#include "utility/dynamictype.hpp"

#include <string>

namespace solid{
namespace frame{

struct Message: Dynamic<Message>{
	Message();
	virtual ~Message();
};

}//namespace frame
}//namespace solid

#endif
