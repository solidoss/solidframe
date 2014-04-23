// audit/log/logrecorders.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef AUDIT_LOGRECORDERS_HPP
#define AUDIT_LOGRECORDERS_HPP

#include <fstream>
#include "audit/log/logrecorder.hpp"

namespace solid{
namespace audit{
//! A simple file recorder
class LogFileRecorder: public LogRecorder{
public:
	LogFileRecorder(const char*_name = NULL, bool _dorespin = false):respin(_dorespin ? 0 : -1){
		if(_name) name = _name;
	}
	bool open(const char *_name = NULL);
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
}//namespace solid

#endif
