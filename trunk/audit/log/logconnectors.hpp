#ifndef AUDIT_LOGCONNECTORS_HPP
#define AUDIT_LOGCONNECTORS_HPP

#include "audit/log/logconnector.hpp"

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

#endif
