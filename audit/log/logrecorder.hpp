// audit/log/logrecorder.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef AUDIT_LOGRECORDER_HPP
#define AUDIT_LOGRECORDER_HPP

#include <vector>

namespace solid{
namespace audit{

class LogRecorder;
struct LogRecorderVector: std::vector<LogRecorder*>{
};

struct LogRecord;
struct LogClientData;

//! An abstract log writer
class LogRecorder{
public:
	virtual ~LogRecorder();
	//! Write a log record to a destination
	/*!
		\param _rcd A reference to clientdata identifying the client
		\param _rrec A reference to a logrecord structure containing record data
	*/
	virtual void record(const LogClientData &_rcd, const LogRecord &_rrec) = 0;
};

}//namespace audit
}//namespace solid

#endif
