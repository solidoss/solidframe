// audit/log/logrecorders.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "audit/log/logrecorders.hpp"
#include "audit/log/logclientdata.hpp"
#include "audit/log/logrecord.hpp"
#include "audit/log.hpp"
#include <ctime>
#include <string>
using namespace std;

namespace solid{
namespace audit{

//--------------------------------------------------------
LogRecorder::~LogRecorder(){
}
//--------------------------------------------------------
bool LogFileRecorder::open(const char *_name){
	if(_name){
		name = _name;
	}
	string path = name;
	path += ".txt";
	ofs.open(path.c_str());
	return true;
}
LogFileRecorder::~LogFileRecorder(){
}

static const char* levelName(unsigned _lvl){
	switch(_lvl){
		case Log::Warn:		return "WAR";
		case Log::Info:		return "INF";
		case Log::Error:	return "ERR";
		case Log::Debug:	return "DEB";
		case Log::Input:	return "INP";
		case Log::Output:	return "OUT";
		default: return "UNK";
	}
}

/*virtual*/ void LogFileRecorder::record(const LogClientData &_rcd, const LogRecord &_rrec){
	char	buf[128];
	time_t	sec = _rrec.head.sec;
	tm		*ploctm;
#ifdef ON_WINDOWS
	ploctm = localtime(&sec);
#else
	tm		loctm;
	ploctm = localtime_r(&sec, &loctm);
#endif
	sprintf(
		buf,
		"%s[%04u-%02u-%02u %02u:%02u:%02u.%03u]",
		levelName(_rrec.head.level),
		ploctm->tm_year + 1900,
		ploctm->tm_mon + 1, 
		ploctm->tm_mday,
		ploctm->tm_hour,
		ploctm->tm_min,
		ploctm->tm_sec,
		_rrec.head.nsec/1000000
	);
	
	ofs<<buf<<'['<<_rcd.modulenamev[_rrec.head.module]<<']';
	if(_rrec.head.level < Log::Input)
		ofs<<'['<<_rrec.fileName()<<'('<<_rrec.head.lineno<<')'<<' '<<_rrec.functionName()<<']';
	ofs<<'['<<_rrec.head.id<<']'<<' ';
	ofs.write(_rrec.data(), _rrec.dataSize())<<endl;
	//ofs.flush();
}


}//namespace audit
}//namespace solid