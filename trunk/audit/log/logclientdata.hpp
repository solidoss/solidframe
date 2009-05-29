#ifndef AUDIT_LOGCLIENTDATA_HPP
#define AUDIT_LOGCLIENTDATA_HPP

#include "audit/log/logdata.hpp"
#include <string>
#include <vector>

namespace audit{
//! Log data identifing a client for log server
struct LogClientData{
	typedef std::vector<std::string> NameVectorTp;
	LogClientData():idx(-1), uid(-1){}
	uint32			idx;
	uint32			uid;
	LogHead			head;
	std::string		procname;
	NameVectorTp	modulenamev;
};

}//namespace audit

#endif
