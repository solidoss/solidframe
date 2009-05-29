#ifndef AUDIT_LOGMANAGER_HPP
#define AUDIT_LOGMANAGER_HPP

#include <utility>
#include "system/common.hpp"

class IStream;

namespace audit{

class LogConnector;


//! The main class for log management
class LogManager{
public:
	typedef std::pair<uint32, uint32> UidTp;
	LogManager();
	~LogManager();
	
	UidTp insertChannel(IStream *_pis);
	UidTp insertListener(const char *_addr, const char *_port);
	
	void eraseClient(const UidTp &_ruid);
	void eraseListener(const UidTp &_ruid);
	
	int start();
	void stop(bool _wait = true);
	UidTp insertConnector(LogConnector *_plc);
	void eraseConnector(const UidTp &_ruid);
private:
	struct ListenerWorker;
	struct ChannelWorker;
	friend class ListenerWorker;
	friend class ChannelWorker;
private:
	void runListener(ListenerWorker &_w);
	void runChannel(ChannelWorker &_w);
private:
	struct Data;
	Data	&d;
};

}//namespace audit

#endif
