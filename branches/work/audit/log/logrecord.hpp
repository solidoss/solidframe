// audit/log/logrecord.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef AUDIT_LOGRECORD_HPP
#define AUDIT_LOGRECORD_HPP

#include "audit/log/logdata.hpp"

namespace solid{
namespace audit{

//! A log record class
struct LogRecord{
	LogRecord():d(NULL), sz(0), cp(0){}
	LogRecordHead head;
	//! Record data with buffer resize
	char* data(uint32 _sz);
	//! the record' source file name
	const char*fileName()const{return d;}
	//! the record' source function name
	const char*functionName()const{return d + head.filenamelen;}
	//! record's text data
	const char*data()const{return d + head.filenamelen + head.functionnamelen;}
	//! record text data size
	uint32 dataSize()const{return sz - head.filenamelen - head.functionnamelen;}
	//! record size
	uint32 size()const{return sz;} 
	//! set the record size
	uint32 size(uint32 _sz){sz = _sz; return _sz;}
	//! get the record buffer capacity
	uint32 capacity()const{return cp;}
private:
	char	*d;
	uint32	sz;
	uint32	cp;
};

}//namespace audit
}//namespace solid

#endif
