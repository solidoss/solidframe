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
//! A class for logging
/*!
	This is not to be used directly.
	It is to be used through preprocessor macros:<br>
	ilog (info log), elog (error log), wlog (warn log), dlog (debug log).
	Here's how it should be used:
	At the begginging of your application do:
	<code>
	Log::instance().reinit(argv[0], Log::AllLevels, "any");<br>
	</code>
	<br>
	then we need to tell the log where to send logging records. In the
	following example we do this on a socket:
	<code><br>
	{<br>
		SocketOStream	*pos(new SocketOStream);<br>
		AddrInfo ai("localhost", 3333, 0, AddrInfo::Inet4, AddrInfo::Stream);<br>
		if(!ai.empty()){<br>
			pos->sd.create(ai.begin());<br>
			pos->sd.connect(ai.begin());<br>
			<br>
			Log::instance().reinit(pos);<br>
			<br>
			idbg("Logging");<br>
		}else{<br>
			delete pos;<br>
			edbg("Sorry no logging");<br>
		}<br>
	}<br>
	</code><br>
	Here's how you specify a log record:
	<code><br>
	ilog(Log::any, 0, "some message");<br>
	elog(Log::any, 1, "some message "<<i<<' '<<some_string);<br>
	</code><br>
	the first parameter of the macro is a module id,
	usually a static const unsigned value initialized like this:
	<code><br>
	static const unsigned imap(Log::instance().registerModule("IMAP");<br>
	</code><br>
	The second parameter is an interger with a local meaning for the module.
	E.g. for the above IMAP module, it can be the unique id of the current connection.
	The last parameter means formated data to be writen. It is just like writing to
	a stl stream.
*/
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
