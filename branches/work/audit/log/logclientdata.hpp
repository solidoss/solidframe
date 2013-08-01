// audit/log/logclientdata.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
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
