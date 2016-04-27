// frame/ipc/src/ipcerror.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "frame/ipc/ipcerror.hpp"
#include <sstream>

namespace solid{
namespace frame{
namespace ipc{

namespace{
enum {
	ErrorInactivityTimeoutE = 1,
	ErrorTooManyKAPacketsReceivedE,
	ErrorConnectionKilledE,
	ErrorLibraryLogicE,
	ErrorConnectionDelayedClosedE,
	ErrorDelayedClosePendingE,
	ErrorMessageCanceledE,
	ErrorConnectionEnterActiveE,
	ErrorConnectionStoppingE,
	ErrorConnectionInvalidStateE,
	ErrorCompressionUnavailableE,
	ErrorReaderInvalidPacketHeaderE,
	ErrorReaderInvalidMessageSwitchE,
	ErrorReaderTooManyMultiplexE,
	ErrorServiceStoppingE,
	ErrorServiceUnknownMessageTypeE,
	ErrorServiceServerOnlyE,
	ErrorServiceUnknownRecipientE,
	ErrorServiceUnknownPoolE,
	ErrorServicePoolStoppingE,
	ErrorServicePoolFullE,
	ErrorServiceUnknownConnectionE,
	ErrorServiceTooManyActiveConnectionsE,
	ErrorServiceBadCastRequestE,
	ErrorServiceBadCastResponseE,
	ErrorServiceStartE,
	ErrorServiceStartListenerE,
	ErrorServiceMessageAlreadyCanceledE,
	ErrorServiceMessageLostE,
	ErrorServiceUnknownMessageE,
};
class ErrorCategory: public ErrorCategoryT
{     
public: 
	const char* name() const noexcept{
		return "frame::ipc";
	}
	std::string message(int _ev)const;
};

const ErrorCategory category;


std::string ErrorCategory::message(int _ev) const{
	std::ostringstream oss;
	
	oss<<"("<<name()<<":"<<_ev<<"): ";
	switch(_ev){
		case 0:
			oss<<"Success";
			break;
		case ErrorInactivityTimeoutE:
			oss<<"Timeout due to inactivity";
			break;
		case ErrorTooManyKAPacketsReceivedE:
			oss<<"Received too many KeepAlive packets";
			break;
		case ErrorConnectionKilledE:
			oss<<"Connection forcefully killed";
			break;
		case ErrorLibraryLogicE:
			oss<<"Library logic error";
			break;
		case ErrorConnectionDelayedClosedE:
			oss<<"Connection delayed closed";
			break;
		case ErrorDelayedClosePendingE:
			oss<<"Delayed Close is pending";
			break;
		case ErrorMessageCanceledE:
			oss<<"Message canceled";
			break;
		case ErrorConnectionEnterActiveE:
			oss<<"Connection cannot enter active state - too many active connections";
			break;
		case ErrorConnectionStoppingE:
			oss<<"Connection is stopping";
			break;
		case ErrorConnectionInvalidStateE:
			oss<<"Connection is a state invalid for opperation";
			break;
		case ErrorCompressionUnavailableE:
			oss<<"Compression support is unavailable";
			break;
		case ErrorReaderInvalidPacketHeaderE:
			oss<<"Reader: invalid packet header";
			break;
		case ErrorReaderInvalidMessageSwitchE:
			oss<<"Reader: invalid message switch";
			break;
		case ErrorReaderTooManyMultiplexE:
			oss<<"Reader: too many multiplexed messages";
			break;
		case ErrorServiceStoppingE:
			oss<<"Service: stopping";
			break;
		case ErrorServiceUnknownMessageTypeE:
			oss<<"Service: unknown message type";
			break;
		case ErrorServiceServerOnlyE:
			oss<<"Service: server only";
			break;
		case ErrorServiceUnknownRecipientE:
			oss<<"Service: unknown recipient";
			break;
		case ErrorServiceUnknownPoolE:
			oss<<"Service: unknown pool";
			break;
		case ErrorServicePoolStoppingE:
			oss<<"Service: pool stopping";
			break;
		case ErrorServicePoolFullE:
			oss<<"Service: pool full";
			break;
		case ErrorServiceUnknownConnectionE:
			oss<<"Service: unknown connection";
			break;
		case ErrorServiceTooManyActiveConnectionsE:
			oss<<"Service: too many active connections";
			break;
		case ErrorServiceBadCastRequestE:
			oss<<"Service: bad cast request message";
			break;
		case ErrorServiceBadCastResponseE:
			oss<<"Service: bad cast response message";
			break;
		case ErrorServiceStartE:
			oss<<"Service: starting service";
			break;
		case ErrorServiceStartListenerE:
			oss<<"Service: starting listener";
			break;
		case ErrorServiceMessageAlreadyCanceledE:
			oss<<"Service: message already canceled";
			break;
		case ErrorServiceMessageLostE:
			oss<<"Service: message lost";
			break;
		case ErrorServiceUnknownMessageE:
			oss<<"Service: unknown message";
			break;
		default:
			oss<<"Unknown";
			break;
	}
	return oss.str();
}

}//namespace

/*extern*/ const ErrorConditionT error_inactivity_timeout(ErrorInactivityTimeoutE, category);
/*extern*/ const ErrorConditionT error_too_many_keepalive_packets_received(ErrorTooManyKAPacketsReceivedE, category);
/*extern*/ const ErrorConditionT error_connection_killed(ErrorConnectionKilledE, category);
/*extern*/ const ErrorConditionT error_library_logic(ErrorLibraryLogicE, category);
/*extern*/ const ErrorConditionT error_connection_delayed_closed(ErrorConnectionDelayedClosedE, category);
/*extern*/ const ErrorConditionT error_delayed_closed_pending(ErrorDelayedClosePendingE, category);
/*extern*/ const ErrorConditionT error_message_canceled(ErrorMessageCanceledE, category);
/*extern*/ const ErrorConditionT error_connection_enter_active(ErrorConnectionEnterActiveE, category);
/*extern*/ const ErrorConditionT error_connection_stopping(ErrorConnectionStoppingE, category);
/*extern*/ const ErrorConditionT error_connection_invalid_state(ErrorConnectionInvalidStateE, category);
/*extern*/ const ErrorConditionT error_compression_unavailable(ErrorCompressionUnavailableE, category);
/*extern*/ const ErrorConditionT error_reader_invalid_packet_header(ErrorReaderInvalidPacketHeaderE, category);
/*extern*/ const ErrorConditionT error_reader_invalid_message_switch(ErrorReaderInvalidMessageSwitchE, category);
/*extern*/ const ErrorConditionT error_reader_too_many_multiplex(ErrorReaderTooManyMultiplexE, category);;
/*extern*/ const ErrorConditionT error_service_stopping(ErrorServiceStoppingE, category);
/*extern*/ const ErrorConditionT error_service_unknown_message_type(ErrorServiceUnknownMessageTypeE, category);
/*extern*/ const ErrorConditionT error_service_server_only(ErrorServiceServerOnlyE, category);
/*extern*/ const ErrorConditionT error_service_unknown_recipient(ErrorServiceUnknownRecipientE, category);
/*extern*/ const ErrorConditionT error_service_unknown_pool(ErrorServiceUnknownPoolE, category);
/*extern*/ const ErrorConditionT error_service_pool_stopping(ErrorServicePoolStoppingE, category);
/*extern*/ const ErrorConditionT error_service_pool_full(ErrorServicePoolFullE, category);
/*extern*/ const ErrorConditionT error_service_unknown_connection(ErrorServiceUnknownConnectionE, category);
/*extern*/ const ErrorConditionT error_service_too_many_active_connections(ErrorServiceTooManyActiveConnectionsE, category);

/*extern*/ const ErrorConditionT error_service_bad_cast_request(ErrorServiceBadCastRequestE, category);
/*extern*/ const ErrorConditionT error_service_bad_cast_response(ErrorServiceBadCastResponseE, category);
/*extern*/ const ErrorConditionT error_service_start(ErrorServiceStartE, category);
/*extern*/ const ErrorConditionT error_service_start_listener(ErrorServiceStartListenerE, category);
/*extern*/ const ErrorConditionT error_service_message_already_canceled(ErrorServiceMessageAlreadyCanceledE, category);
/*extern*/ const ErrorConditionT error_service_message_lost(ErrorServiceMessageLostE, category);
/*extern*/ const ErrorConditionT error_service_unknown_message(ErrorServiceUnknownMessageE, category);

}//namespace ipc
}//namespace frame
}//namespace solid


