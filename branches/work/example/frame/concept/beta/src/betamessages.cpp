// betasignals.cpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "beta/betamessages.hpp"


namespace concept{
namespace beta{
size_t LoginMessage::release(){
	return 1;
}

size_t CancelMessage::release(){
	return 1;
}

}
}
