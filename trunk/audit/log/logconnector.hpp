#ifndef AUDIT_LOGCONNECTOR_HPP
#define AUDIT_LOGCONNECTOR_HPP

#include "system/common.hpp"

namespace audit{

struct LogRecorderVector;
struct LogRecord;
struct LogClientData;

//! A log connector is a container of log client connections and a filter for log records
/*!
	Inherit this class and implement the actual connection keeping and the filtering.
*/
class LogConnector{
public:
	LogConnector();
	virtual ~LogConnector();
	bool destroy();
	bool receivePrepare(LogRecorderVector& _outrv, const LogRecord &_rrec, const LogClientData &_rcl);
	bool receiveDone();
	virtual void eraseClient(uint32 _idx, uint32 _uid) = 0;
protected:
	bool mustDie()const{return mustdie;}
	//! Implement this method to filter and/or dispatch records
	virtual void doReceive(LogRecorderVector& _outrv, const LogRecord &_rrec, const LogClientData &_rcl) = 0;
private:
	uint	usecnt;
	bool	mustdie;
};

}//namespace audit

#endif
