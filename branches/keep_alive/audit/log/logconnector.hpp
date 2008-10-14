#ifndef AUDIT_LOGCONNECTOR_HPP
#define AUDIT_LOGCONNECTOR_HPP

namespace audit{

class RecorderVector;
class LogRecord;
class LogClientData;

class LogConnector{
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
