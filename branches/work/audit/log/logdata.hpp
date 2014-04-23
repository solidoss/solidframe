// audit/log/logdata.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef AUDIT_LOGDATA_HPP
#define AUDIT_LOGDATA_HPP

#include "system/convertors.hpp"

namespace solid{
namespace audit{
//! The head of a log record - sent from client to server
struct LogRecordHead{
	uint16		level;
	uint16		module;
	uint32		id;
	uint32		lineno;
	uint32		sec;
	uint32		nsec;
	uint16		filenamelen;
	uint16		functionnamelen;
	uint32		datalen;
	
	void set(uint16 _lvl, uint16 _mod, uint32 _id, uint32 _lineno, uint32 _sec, uint32 _nsec){
		level = toNetwork(_lvl);
		module = toNetwork(_mod);
		id = toNetwork(_id);
		lineno = toNetwork(_lineno);
		sec = toNetwork(_sec);
		nsec = toNetwork(_nsec);
	}
	void set(uint16 _filenamelen, uint16 _functionnamelen){
		filenamelen = toNetwork(_filenamelen);
		functionnamelen = toNetwork(_functionnamelen);
	}
	void dataLength(uint32 _datalen){
		datalen = toNetwork(_datalen);
	}
	void convertToHost(){
		level = toHost(level);
		module = toHost(module);
		id = toHost(id);
		lineno = toHost(lineno);
		sec = toHost(sec);
		nsec = toHost(nsec);
		filenamelen = toHost(filenamelen);
		functionnamelen = toHost(functionnamelen);
		datalen = toHost(datalen);
	}
};
//! The head of a log connection - sent from client to server
struct LogHead{
	LogHead(
		uint32 _procid = 0,
		uint16 _procnamelen = 0,
		uint16 _modulecnt = 0
	):procid(_procid), version(1), procnamelen(_procnamelen),
		 modulecnt(_modulecnt), flags(0xffff){}
	
	void convertToNetwork(){
		version = toNetwork(version);
		procnamelen = toNetwork(procnamelen);
		procid = toNetwork(procid);
		modulecnt = toNetwork(modulecnt);
	}
	void convertToHost(){
		version = toHost(version);
		procnamelen = toHost(procnamelen);
		procid = toHost(procid);
		modulecnt = toHost(modulecnt);
	}
	
	uint32	procid;
	uint16	version;
	uint16	procnamelen;
	uint16	modulecnt;
	uint16	flags;
};

}//namespace audit
}//namespace solid


#endif
