// solid/frame/ipc/src/ipcerror.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/frame/mprpc/mprpcerror.hpp"
#include <sstream>

namespace solid {
namespace frame {
namespace mprpc {

namespace {

enum {
    ErrorConnectionInactivityTimeoutE = 1,
    ErrorConnectionTooManyKAPacketsReceivedE,
    ErrorConnectionTooManyRecvBuffersE,
    ErrorConnectionKilledE,
    ErrorConnectionLogicE,
    ErrorConnectionResolveE,
    ErrorConnectionDelayedClosedE,
    ErrorConnectionMessageCanceledE,
    ErrorConnectionEnterActiveE,
    ErrorConnectionStoppingE,
    ErrorConnectionInvalidStateE,
    ErrorConnectionNoSecureConfigurationE,
    ErrorConnectionAckCountE,
    ErrorConnectionInvalidResponseStateE,
    ErrorMessageCanceledE,
    ErrorMessageCanceledPeerE,
    ErrorMessageConnectionE,
    ErrorCompressionUnavailableE,
    ErrorCompressionEngineE,
    ErrorReaderInvalidPacketHeaderE,
    ErrorReaderInvalidMessageSwitchE,
    ErrorReaderTooManyMultiplexE,
    ErrorReaderProtocolE,
    ErrorServiceStoppingE,
    ErrorServiceMessageUnknownTypeE,
    ErrorServiceMessageFlagsE,
    ErrorServiceMessageStateE,
    ErrorServiceMessageNullE,
    ErrorServiceServerOnlyE,
    ErrorServiceUnknownRecipientE,
    ErrorServicePoolExistsE,
    ErrorServicePoolUnknownE,
    ErrorServicePoolStoppingE,
    ErrorServicePoolFullE,
    ErrorServiceUnknownConnectionE,
    ErrorServiceTooManyActiveConnectionsE,
    ErrorServiceAlreadyActiveE,
    ErrorServiceBadCastRequestE,
    ErrorServiceBadCastResponseE,
    ErrorServiceStartE,
    ErrorServiceStartListenerE,
    ErrorServiceMessageAlreadyCanceledE,
    ErrorServiceMessageLostE,
    ErrorServiceUnknownMessageE,
    ErrorServiceInvalidUrlE,
    ErrorServiceConnectionNotNeededE,
};

class ErrorCategory : public ErrorCategoryT {
public:
    ErrorCategory() {}
    const char* name() const noexcept override
    {
        return "solid::frame::mprpc";
    }
    std::string message(int _ev) const override;
};

const ErrorCategory category;

std::string ErrorCategory::message(int _ev) const
{
    std::ostringstream oss;

    oss << "(" << name() << ":" << _ev << "): ";

    switch (_ev) {
    case 0:
        oss << "Success";
        break;
    case ErrorConnectionInactivityTimeoutE:
        oss << "Connection: Timeout due to inactivity";
        break;
    case ErrorConnectionTooManyKAPacketsReceivedE:
        oss << "Connection: Received too many KeepAlive packets";
        break;
    case ErrorConnectionTooManyRecvBuffersE:
        oss << "Connection: Too many recv buffers";
        break;
    case ErrorConnectionKilledE:
        oss << "Connection: killed";
        break;
    case ErrorConnectionLogicE:
        oss << "Connection: logic";
        break;
    case ErrorConnectionResolveE:
        oss << "Connection: resolve recipient name";
        break;
    case ErrorConnectionDelayedClosedE:
        oss << "Connection delayed closed";
        break;
    case ErrorConnectionAckCountE:
        oss << "Connection: invalid ACK count received";
        break;
    case ErrorMessageCanceledE:
        oss << "Message canceled";
        break;
    case ErrorMessageCanceledPeerE:
        oss << "Message canceled by peer";
        break;
    case ErrorMessageConnectionE:
        oss << "Message connection";
        break;
    case ErrorConnectionEnterActiveE:
        oss << "Connection cannot enter active state - too many active connections";
        break;
    case ErrorConnectionStoppingE:
        oss << "Connection is stopping";
        break;
    case ErrorConnectionInvalidStateE:
        oss << "Connection is a state invalid for opperation";
        break;
    case ErrorConnectionNoSecureConfigurationE:
        oss << "Connection: no secure configuration";
        break;
    case ErrorConnectionInvalidResponseStateE:
        oss << "Connection: invalid response state";
        break;
    case ErrorCompressionUnavailableE:
        oss << "Compression support is unavailable";
        break;
    case ErrorCompressionEngineE:
        oss << "Compression engine error";
        break;
    case ErrorReaderInvalidPacketHeaderE:
        oss << "Reader: invalid packet header";
        break;
    case ErrorReaderInvalidMessageSwitchE:
        oss << "Reader: invalid message switch";
        break;
    case ErrorReaderTooManyMultiplexE:
        oss << "Reader: too many multiplexed messages";
        break;
    case ErrorReaderProtocolE:
        oss << "Reader: protocol";
        break;
    case ErrorServiceStoppingE:
        oss << "Service: stopping";
        break;
    case ErrorServiceMessageUnknownTypeE:
        oss << "Service: message unknown type";
        break;
    case ErrorServiceMessageFlagsE:
        oss << "Service: message flags";
        break;
    case ErrorServiceMessageStateE:
        oss << "Service: message state";
        break;
    case ErrorServiceMessageNullE:
        oss << "Service: message null";
        break;
    case ErrorServiceServerOnlyE:
        oss << "Service: server only";
        break;
    case ErrorServiceUnknownRecipientE:
        oss << "Service: unknown recipient";
        break;
    case ErrorServicePoolUnknownE:
        oss << "Service: pool unknown";
        break;
    case ErrorServicePoolExistsE:
        oss << "Service: pool already exists";
        break;
    case ErrorServicePoolStoppingE:
        oss << "Service: pool stopping";
        break;
    case ErrorServicePoolFullE:
        oss << "Service: pool full";
        break;
    case ErrorServiceUnknownConnectionE:
        oss << "Service: unknown connection";
        break;
    case ErrorServiceTooManyActiveConnectionsE:
        oss << "Service: too many active connections";
        break;
    case ErrorServiceAlreadyActiveE:
        oss << "Service: connection already active";
        break;
    case ErrorServiceBadCastRequestE:
        oss << "Service: bad cast request message";
        break;
    case ErrorServiceBadCastResponseE:
        oss << "Service: bad cast response message";
        break;
    case ErrorServiceStartE:
        oss << "Service: starting service";
        break;
    case ErrorServiceStartListenerE:
        oss << "Service: starting listener";
        break;
    case ErrorServiceMessageAlreadyCanceledE:
        oss << "Service: message already canceled";
        break;
    case ErrorServiceMessageLostE:
        oss << "Service: message lost";
        break;
    case ErrorServiceUnknownMessageE:
        oss << "Service: unknown message";
        break;
    case ErrorServiceInvalidUrlE:
        oss << "Service: invalid URL";
        break;
    case ErrorServiceConnectionNotNeededE:
        oss << "Service: connection not needed";
        break;
    default:
        oss << "Unknown";
        break;
    }
    return oss.str();
}

} // namespace

/*extern*/ const ErrorConditionT error_connection_inactivity_timeout(ErrorConnectionInactivityTimeoutE, category);
/*extern*/ const ErrorConditionT error_connection_too_many_keepalive_packets_received(ErrorConnectionTooManyKAPacketsReceivedE, category);
/*extern*/ const ErrorConditionT error_connection_too_many_recv_buffers(ErrorConnectionTooManyRecvBuffersE, category);
/*extern*/ const ErrorConditionT error_connection_killed(ErrorConnectionKilledE, category);
/*extern*/ const ErrorConditionT error_connection_logic(ErrorConnectionLogicE, category);
/*extern*/ const ErrorConditionT error_connection_resolve(ErrorConnectionResolveE, category);
/*extern*/ const ErrorConditionT error_connection_delayed_closed(ErrorConnectionDelayedClosedE, category);
/*extern*/ const ErrorConditionT error_connection_enter_active(ErrorConnectionEnterActiveE, category);
/*extern*/ const ErrorConditionT error_connection_stopping(ErrorConnectionStoppingE, category);
/*extern*/ const ErrorConditionT error_connection_invalid_state(ErrorConnectionInvalidStateE, category);
/*extern*/ const ErrorConditionT error_connection_no_secure_configuration(ErrorConnectionNoSecureConfigurationE, category);
/*extern*/ const ErrorConditionT error_connection_ack_count(ErrorConnectionAckCountE, category);
/*extern*/ const ErrorConditionT error_connection_invalid_response_state(ErrorConnectionInvalidResponseStateE, category);

/*extern*/ const ErrorConditionT error_message_canceled(ErrorMessageCanceledE, category);
/*extern*/ const ErrorConditionT error_message_canceled_peer(ErrorMessageCanceledPeerE, category);
/*extern*/ const ErrorConditionT error_message_connection(ErrorMessageConnectionE, category);

/*extern*/ const ErrorConditionT error_compression_unavailable(ErrorCompressionUnavailableE, category);
/*extern*/ const ErrorConditionT error_compression_engine(ErrorCompressionEngineE, category);

/*extern*/ const ErrorConditionT error_reader_invalid_packet_header(ErrorReaderInvalidPacketHeaderE, category);
/*extern*/ const ErrorConditionT error_reader_invalid_message_switch(ErrorReaderInvalidMessageSwitchE, category);
/*extern*/ const ErrorConditionT error_reader_too_many_multiplex(ErrorReaderTooManyMultiplexE, category);
/*extern*/ const ErrorConditionT error_reader_protocol(ErrorReaderProtocolE, category);

/*extern*/ const ErrorConditionT error_service_stopping(ErrorServiceStoppingE, category);
/*extern*/ const ErrorConditionT error_service_message_unknown_type(ErrorServiceMessageUnknownTypeE, category);
/*extern*/ const ErrorConditionT error_service_message_flags(ErrorServiceMessageFlagsE, category);
/*extern*/ const ErrorConditionT error_service_message_state(ErrorServiceMessageStateE, category);
/*extern*/ const ErrorConditionT error_service_message_null(ErrorServiceMessageNullE, category);
/*extern*/ const ErrorConditionT error_service_server_only(ErrorServiceServerOnlyE, category);
/*extern*/ const ErrorConditionT error_service_unknown_recipient(ErrorServiceUnknownRecipientE, category);
/*extern*/ const ErrorConditionT error_service_pool_unknown(ErrorServicePoolUnknownE, category);
/*extern*/ const ErrorConditionT error_service_pool_exists(ErrorServicePoolExistsE, category);
/*extern*/ const ErrorConditionT error_service_pool_stopping(ErrorServicePoolStoppingE, category);
/*extern*/ const ErrorConditionT error_service_pool_full(ErrorServicePoolFullE, category);
/*extern*/ const ErrorConditionT error_service_unknown_connection(ErrorServiceUnknownConnectionE, category);
/*extern*/ const ErrorConditionT error_service_too_many_active_connections(ErrorServiceTooManyActiveConnectionsE, category);
/*extern*/ const ErrorConditionT error_service_already_active(ErrorServiceAlreadyActiveE, category);

/*extern*/ const ErrorConditionT error_service_bad_cast_request(ErrorServiceBadCastRequestE, category);
/*extern*/ const ErrorConditionT error_service_bad_cast_response(ErrorServiceBadCastResponseE, category);
/*extern*/ const ErrorConditionT error_service_start(ErrorServiceStartE, category);
/*extern*/ const ErrorConditionT error_service_start_listener(ErrorServiceStartListenerE, category);
/*extern*/ const ErrorConditionT error_service_message_already_canceled(ErrorServiceMessageAlreadyCanceledE, category);
/*extern*/ const ErrorConditionT error_service_message_lost(ErrorServiceMessageLostE, category);
/*extern*/ const ErrorConditionT error_service_unknown_message(ErrorServiceUnknownMessageE, category);
/*extern*/ const ErrorConditionT error_service_invalid_url(ErrorServiceInvalidUrlE, category);
/*extern*/ const ErrorConditionT error_service_connection_not_needed(ErrorServiceConnectionNotNeededE, category);

} // namespace mprpc
} // namespace frame
} // namespace solid
