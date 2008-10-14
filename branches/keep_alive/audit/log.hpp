/* Declarations file log.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef AUDIT_LOG_HPP
#define AUDIT_LOG_HPP

#include "system/common.hpp"
#include <string>
#include <ostream>

#define LOG_BITSET_SIZE 256

class OStream;

class Log{
public:
	enum Level{
		Info = 1,
		Error = 2,
		Warn = 4,
		Debug = 8,
		Input = 16,
		Output = 32,
		AllLevels = 0xffff
	};
	
	static const unsigned any;
	
	static Log& instance();
	
	bool reinit(const char* _procname, uint32 _lvlmsk = 0, const char *_modopt = NULL, OStream *_pos = NULL);
	
	void reinit(OStream *_pos);
	
	void moduleNames(std::string &_ros);
	void setAllModuleBits();
	void resetAllModuleBits();
	void setModuleBit(unsigned _v);
	void resetModuleBit(unsigned _v);
	unsigned registerModule(const char *_name);
	
	std::ostream& record(
		Level _lvl,
		unsigned _module,
		uint32	_id,
		const char *_file,
		const char *_fnc,
		int _line
	);
	void done();
	bool isSet(unsigned _mod, unsigned _level)const;
private:
	Log();
	~Log();
	struct Data;
	Data	&d;
};

#define ilog(a,id,x)\
	if(Log::instance().isSet(a, Log::Info)){\
		Log::instance().record(Log::Info, a, id, __FILE__, __FUNCTION__, __LINE__)<<x;Log::instance().done();}

#define elog(a,id,x)\
	if(Log::instance().isSet(a, Log::Error)){\
		Log::instance().record(Log::Error, a, id,  __FILE__, __FUNCTION__, __LINE__)<<x;Log::instance().done();}

#define wlog(a,id,x)\
	if(Log::instance().isSet(a, Log::Warn)){\
		Log::instance().record(Log::Warn, a, id,  __FILE__, __FUNCTION__, __LINE__)<<x;Log::instance().done();}

#define dlog(a,id,x)\
	if(Log::instance().isSet(a, Log::Debug)){\
		Log::instance().record(Log::Debug, a, id,  __FILE__, __FUNCTION__, __LINE__)<<x;Log::instance().done();}

#endif
