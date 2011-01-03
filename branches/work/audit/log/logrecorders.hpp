#ifndef AUDIT_LOGRECORDERS_HPP
#define AUDIT_LOGRECORDERS_HPP

#include <fstream>
#include "audit/log/logrecorder.hpp"

namespace audit{
//! A simple file recorder
class LogFileRecorder: public LogRecorder{
public:
	LogFileRecorder(const char*_name = NULL, bool _dorespin = false):respin(_dorespin ? 0 : -1){
		if(_name) name = _name;
	}
	int open(const char *_name = NULL);
	~LogFileRecorder();
	/*virtual*/ void record(const LogClientData &_rcd, const LogRecord &_rrec);
	const std::string& fileName()const{return name;}
	std::string& fileName(){return name;}
protected:
	std::ofstream	ofs;
	std::string		name;
	int				respin;
};

}//namespace audit

#endif
