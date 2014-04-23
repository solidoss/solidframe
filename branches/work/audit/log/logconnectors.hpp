// audit/log/logconnectors.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef AUDIT_LOGCONNECTORS_HPP
#define AUDIT_LOGCONNECTORS_HPP

#include "audit/log/logconnector.hpp"

namespace solid{
namespace audit{

class LogRecorder;
//! A basic log connector
class LogBasicConnector: public LogConnector{
public:
	LogBasicConnector(const char *_prfx = NULL);
	void prefix(const char *_prfx);
	~LogBasicConnector();
protected:
	/*virtual*/ void eraseClient(uint32 _idx, uint32 _uid);
	/*virtual*/ void doReceive(LogRecorderVector& _outrv, const LogRecord &_rrec, const LogClientData &_rcl);
	LogRecorder* createRecorder(const LogClientData &_rcl);
private:
	struct Data;
	Data	&d;
};


}//namespace audit
}//namespace solid

#endif
