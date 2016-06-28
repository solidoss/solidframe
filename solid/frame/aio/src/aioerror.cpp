// solid/frame/aio/src/aioerror.cpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/frame/aio/aioerror.hpp"
#include <sstream>

namespace solid{
namespace frame{
namespace aio{

namespace{

enum {
	ErrorResolverDirectE = 1,
	ErrorResolverReverseE,
	ErrorAlreadyE,
	ErrorDatagramShutdownE,
	ErrorDatagramSystemE,
	ErrorDatagramCreateE,
	ErrorDatagramSocketE,
	ErrorStreamSystemE,
	ErrorStreamSocketE,
	ErrorStreamShutdownE,
	ErrorTimerCancelE,
	ErrorListenerSystemE,
	ErrorSecureContextE,
	ErrorSecureSocketE,
	ErrorSecureRecvE,
	ErrorSecureSendE,
	ErrorSecureAcceptE,
	ErrorSecureConnectE,
	ErrorSecureShutdownE,
};

class ErrorCategory: public ErrorCategoryT
{     
public:
    ErrorCategory(){}
	const char* name() const noexcept{
		return "frame::aio";
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
		case ErrorResolverDirectE:
			oss<<"Resolver: direct";
			break;
		case ErrorResolverReverseE:
			oss<<"Resolver: reverse";
			break;
		case ErrorAlreadyE:
			oss<<"Opperation already in progress";
			break;
		case ErrorDatagramShutdownE:
			oss<<"Datagram: peer shutdown";
			break;
		case ErrorDatagramSystemE:
			oss<<"Datagram: system";
			break;
		case ErrorDatagramCreateE:
			oss<<"Datagram: socket create";
			break;
		case ErrorDatagramSocketE:
			oss<<"Datagram: socket";
			break;
		case ErrorStreamSystemE:
			oss<<"Stream: system";
			break;
		case ErrorStreamSocketE:
			oss<<"Stream: socket";
			break;
		case ErrorStreamShutdownE:
			oss<<"Stream: peer shutdown";
			break;
		case ErrorTimerCancelE:
			oss<<"Timer: canceled";
			break;
		case ErrorListenerSystemE:
			oss<<"Listener: system";
			break;
		case ErrorSecureContextE:
			oss<<"Secure: context";
			break;
		case ErrorSecureSocketE:
			oss<<"Secure: socket";
			break;
		case ErrorSecureRecvE:
			oss<<"Secure: recv";
			break;
		case ErrorSecureSendE:
			oss<<"Secure: send";
			break;
		case ErrorSecureAcceptE:
			oss<<"Secure: accept";
			break;
		case ErrorSecureConnectE:
			oss<<"Secure: connect";
			break;
		case ErrorSecureShutdownE:
			oss<<"Secure: shutdown";
			break;
		default:
			oss<<"Unknown";
			break;
	}
	return oss.str();
}

}//namespace

/*extern*/ const ErrorCodeT			error_resolver_direct(ErrorResolverDirectE, category);
/*extern*/ const ErrorCodeT			error_resolver_reverse(ErrorResolverReverseE, category);

/*extern*/ const ErrorConditionT	error_already(ErrorAlreadyE, category);

/*extern*/ const ErrorConditionT	error_datagram_shutdown(ErrorDatagramShutdownE, category);
/*extern*/ const ErrorConditionT	error_datagram_system(ErrorDatagramSystemE, category);
/*extern*/ const ErrorConditionT	error_datagram_create(ErrorDatagramCreateE, category);
/*extern*/ const ErrorConditionT	error_datagram_socket(ErrorDatagramSocketE, category);

/*extern*/ const ErrorConditionT	error_stream_system(ErrorStreamSystemE, category);
/*extern*/ const ErrorConditionT	error_stream_socket(ErrorStreamSocketE, category);
/*extern*/ const ErrorConditionT	error_stream_shutdown(ErrorStreamShutdownE, category);

/*extern*/ const ErrorConditionT	error_timer_cancel(ErrorTimerCancelE, category);

/*extern*/ const ErrorConditionT	error_listener_system(ErrorListenerSystemE, category);

/*extern*/ const ErrorCodeT			error_secure_context(ErrorSecureContextE, category);
/*extern*/ const ErrorCodeT			error_secure_socket(ErrorSecureSocketE, category);
/*extern*/ const ErrorCodeT			error_secure_recv(ErrorSecureRecvE, category);
/*extern*/ const ErrorCodeT			error_secure_send(ErrorSecureSendE, category);
/*extern*/ const ErrorCodeT			error_secure_accept(ErrorSecureAcceptE, category);
/*extern*/ const ErrorCodeT			error_secure_connect(ErrorSecureConnectE, category);
/*extern*/ const ErrorCodeT			error_secure_shutdown(ErrorSecureShutdownE, category);

}//namespace aio
}//namespace frame
}//namespace solid


