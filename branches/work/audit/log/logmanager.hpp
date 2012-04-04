#ifndef AUDIT_LOGMANAGER_HPP
#define AUDIT_LOGMANAGER_HPP

#include <utility>
#include "system/common.hpp"

class InputStream;

namespace audit{

class LogConnector;


//! The main class for log management
class LogManager{
public:
	typedef std::pair<uint32, uint32> UidT;
	LogManager();
	~LogManager();
	
	UidT insertChannel(InputStream *_pis);
	UidT insertListener(const char *_addr, const char *_port);
	
	void eraseClient(const UidT &_ruid);
	void eraseListener(const UidT &_ruid);
	
	int start();
	void stop(bool _wait = true);
	UidT insertConnector(LogConnector *_plc);
	void eraseConnector(const UidT &_ruid);
private:
	struct ListenerWorker;
	struct ChannelWorker;
	friend struct ListenerWorker;
	friend struct ChannelWorker;
private:
	void runListener(ListenerWorker &_w);
	void runChannel(ChannelWorker &_w);
private:
	struct Data;
	Data	&d;
};

}//namespace audit

#endif
