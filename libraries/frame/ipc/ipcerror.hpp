// frame/ipc/ipcerror.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_IPCERROR_HPP
#define SOLID_FRAME_IPC_IPCERROR_HPP

#include "system/error.hpp"

namespace solid{
namespace frame{
namespace ipc{

extern const ErrorConditionT error_inactivity_timeout;
extern const ErrorConditionT error_too_many_keepalive_packets_received;
extern const ErrorConditionT error_connection_killed;
extern const ErrorConditionT error_connection_delayed_closed;
extern const ErrorConditionT error_connection_inexistent;
extern const ErrorConditionT error_connection_enter_active;
extern const ErrorConditionT error_connection_stopping;
extern const ErrorConditionT error_delayed_closed_pending;
extern const ErrorConditionT error_library_logic;
extern const ErrorConditionT error_message_canceled;


}//namespace ipc
}//namespace frame
}//namespace solid

#endif
