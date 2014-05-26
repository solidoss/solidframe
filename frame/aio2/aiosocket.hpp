// frame/aio/aioplainsocket.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_PLAINSOCKET_HPP
#define SOLID_FRAME_AIO_PLAINSOCKET_HPP

#include "system/common.hpp"
#include "system/socketdevice.hpp"

namespace solid{
namespace frame{
namespace aio{
	

struct ActionData{
	uint8	want;
};

class BaseSocket{
public:
	size_t	id()const{
		return idx;
	}
protected:
	SocketDevice	dev;
	size_t			idx;
	ActionData		*precvaction;
	ActionData		*psendaction;
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
