// audit/log/logmanager.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef AUDIT_LOGMANAGER_HPP
#define AUDIT_LOGMANAGER_HPP

#include <utility>
#include "system/common.hpp"

namespace solid{
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
	
	bool start();
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
}//namespace solid

#endif
