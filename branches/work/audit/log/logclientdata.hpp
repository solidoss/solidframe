#ifndef AUDIT_LOGCLIENTDATA_HPP
#define AUDIT_LOGCLIENTDATA_HPP

#include "audit/log/logdata.hpp"
#include <string>
#include <vector>

namespace solid{
namespace audit{
//! Log data identifing a client for log server
struct LogClientData{
	typedef std::vector<std::string> NameVectorT;
	LogClientData():idx(-1), uid(-1){}
	uint32			idx;
	uint32			uid;
	LogHead			head;
	std::string		procname;
	NameVectorT		modulenamev;
};

}//namespace audit
}//namespace solid

#endif
