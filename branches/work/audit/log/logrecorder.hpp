#ifndef AUDIT_LOGRECORDER_HPP
#define AUDIT_LOGRECORDER_HPP

#include <vector>

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

#endif
