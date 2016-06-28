// solid/frame/aio/aioerror.hpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_AIOERROR_HPP
#define SOLID_FRAME_AIO_AIOERROR_HPP

#include "solid/system/error.hpp"

namespace solid{
namespace frame{
namespace aio{

extern const ErrorCodeT			error_resolver_direct;
extern const ErrorCodeT			error_resolver_reverse;

extern const ErrorConditionT	error_already;

extern const ErrorConditionT	error_datagram_shutdown;
extern const ErrorConditionT	error_datagram_system;
extern const ErrorConditionT	error_datagram_create;
extern const ErrorConditionT	error_datagram_socket;

extern const ErrorConditionT	error_stream_system;
extern const ErrorConditionT	error_stream_socket;
extern const ErrorConditionT	error_stream_shutdown;

extern const ErrorConditionT	error_timer_cancel;

extern const ErrorConditionT	error_listener_system;

extern const ErrorCodeT			error_secure_context;
extern const ErrorCodeT			error_secure_socket;
extern const ErrorCodeT			error_secure_recv;
extern const ErrorCodeT			error_secure_send;
extern const ErrorCodeT			error_secure_accept;
extern const ErrorCodeT			error_secure_connect;
extern const ErrorCodeT			error_secure_shutdown;

}//namespace aio
}//namespace frame
}//namespace solid

#endif
