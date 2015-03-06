// frame/aio/aioforwardcompletion.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_FORWARD_COMPLETION_HPP
#define SOLID_FRAME_AIO_FORWARD_COMPLETION_HPP

namespace solid{
namespace frame{
namespace aio{

class CompletionHandler;

struct ForwardCompletionHandler{
	ForwardCompletionHandler(CompletionHandler *_pch = nullptr):pnext(_pch){}
	CompletionHandler	*pnext;
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
