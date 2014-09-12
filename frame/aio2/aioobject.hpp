// frame/aio/aioobject.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_OBJECT_HPP
#define SOLID_FRAME_AIO_OBJECT_HPP

#include "system/timespec.hpp"

#include "frame/objectbase.hpp"
#include "frame/aio2/aiocommon.hpp"

namespace solid{

namespace frame{
namespace aio{
class Message;
struct ReactorContext;

class Object: public Dynamic<Object, ObjectBase>{
public:
	typedef DynamicPointer<Message>	MessagePointerT;
	
// 	static Object& specific(){
// 		return static_cast<ThisT&>(ObjectBase::specific());
// 	}
protected:
	//! Constructor
	Object(){}
private:
	virtual bool onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
		return false;
	}
private:
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
