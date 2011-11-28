/* Declarations file debug.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SYSTEM_DEBUG_HPP
#define SYSTEM_DEBUG_HPP


#ifdef USTATISTICS

#define COLLECT_DATA_0(om)\
	om()\


#define COLLECT_DATA_1(om,p1)\
	om(p1)\

#define COLLECT_DATA_2(om,p1,p2)\
	om(p1, p2)\

#else
#define COLLECT_DATA_0(om)

#define COLLECT_DATA_1(om,p1)

#define COLLECT_DATA_2(om,p1,p2)

#endif

#ifdef UDEBUG

#define DEBUG_BITSET_SIZE 256
#include <ostream>
#include <string>
#include "system/common.hpp"


#ifdef ON_WINDOWS

#ifdef DO_EXPORT_DLL
#define EXPORT_DLL __declspec(dllexport)
#else
#define EXPORT_DLL __declspec(dllimport)
#endif

#endif

#ifndef EXPORT_DLL
#define EXPORT_DLL
#endif



struct EXPORT_DLL Dbg{
	static const unsigned any;
	static const unsigned system;
	static const unsigned specific;
	static const unsigned protocol;
	static const unsigned ser_bin;
	static const unsigned utility;
	static const unsigned fdt;
	static const unsigned ipc;
	static const unsigned tcp;
	static const unsigned udp;
	static const unsigned file;
	static const unsigned log;
	static const unsigned aio;
	static Dbg& instance();
	enum Level{
		Info = 1,
		Error = 2,
		Warn = 4,
		Report = 8,
		Verbose = 16,
		Trace = 32,
		AllLevels = 1 + 2 + 4 + 8 + 16 + 32
	};
	
	~Dbg();

	void initStdErr(
		bool _buffered = true,
		std::string *_output = NULL
	);

	void initFile(
		const char * _fname,
		bool _buffered = true,
		ulong _respincnt = 10,
		ulong _respinsize = 1024 * 1024 * 1024,
		std::string *_output = NULL
	);
	void initSocket(
		const char * _addr,
		const char * _port,
		bool _buffered = true,
		std::string *_output = NULL
	);

	void levelMask(const char *_msk = NULL);
	void moduleMask(const char *_msk = NULL);
	
	void moduleBits(std::string &_ros);
	void setAllModuleBits();
	void resetAllModuleBits();
	void setModuleBit(unsigned _v);
	void resetModuleBit(unsigned _v);
	unsigned registerModule(const char *_name);
	std::ostream& print();
	std::ostream& print(
		const char _t,
		const char *_file,
		const char *_fnc,
		int _line
	);
	std::ostream& print(
		const char _t,
		unsigned _module,
		const char *_file,
		const char *_fnc,
		int _line
	);
	std::ostream& printTraceIn(
		const char _t,
		unsigned _module,
		const char *_file,
		const char *_fnc,
		int _line
	);
	std::ostream& printTraceOut(
		const char _t,
		unsigned _module,
		const char *_file,
		const char *_fnc,
		int _line
	);
	void done();
	void doneTraceIn();
	void doneTraceOut();
	bool isSet(Level _lvl, unsigned _v)const;
private:
	Dbg();
	struct Data;
	Data &d;
};

//#ifdef __PRETTY_FUNCTION__
//#define CRT_FUNCTION_NAME __PRETTY_FUNCTION__
// #else
// 
#ifndef __FUNCTION__
#define __FUNCTION__ __func__
#endif

#ifdef UTRACE

struct DbgTraceTest{
	DbgTraceTest(
		int _mod,
		const char *_file,
		const char *_fnc,
		int _line):v(1), mod(_mod), file(_file), fnc(_fnc),line(_line){}
	~DbgTraceTest();
	int			v;
	int			mod;
	const char	*file;
	const char	*fnc;
	int			line;
};

#endif //UTRACE

#define CRT_FUNCTION_NAME __FUNCTION__
// #endif

#define idbg(x)\
	if(Dbg::instance().isSet(Dbg::Info, Dbg::any)){\
	Dbg::instance().print('I', Dbg::any, __FILE__, CRT_FUNCTION_NAME, __LINE__)<<x;Dbg::instance().done();}else;
#define idbgx(a,x)\
	if(Dbg::instance().isSet(Dbg::Info, a)){\
	Dbg::instance().print('I', a,  __FILE__, CRT_FUNCTION_NAME, __LINE__)<<x;Dbg::instance().done();}else;

#define edbg(x)\
	if(Dbg::instance().isSet(Dbg::Error, Dbg::any)){\
	Dbg::instance().print('E', Dbg::any, __FILE__, CRT_FUNCTION_NAME, __LINE__)<<x;Dbg::instance().done();}else;
#define edbgx(a,x)\
	if(Dbg::instance().isSet(Dbg::Error, a)){\
	Dbg::instance().print('E', a,  __FILE__, CRT_FUNCTION_NAME, __LINE__)<<x;Dbg::instance().done();}else;

#define wdbg(x)\
	if(Dbg::instance().isSet(Dbg::Warn, Dbg::any)){\
	Dbg::instance().print('W', Dbg::any, __FILE__, CRT_FUNCTION_NAME, __LINE__)<<x;Dbg::instance().done();}else;
#define wdbgx(a,x)\
	if(Dbg::instance().isSet(Dbg::Warn, a)){\
	Dbg::instance().print('W', a,  __FILE__, CRT_FUNCTION_NAME, __LINE__)<<x;Dbg::instance().done();}else;

#define rdbg(x)\
	if(Dbg::instance().isSet(Dbg::Report, Dbg::any)){\
	Dbg::instance().print('R', Dbg::any, __FILE__, CRT_FUNCTION_NAME, __LINE__)<<x;Dbg::instance().done();}else;
#define rdbgx(a,x)\
	if(Dbg::instance().isSet(Dbg::Report, a)){\
	Dbg::instance().print('R', a,  __FILE__, CRT_FUNCTION_NAME, __LINE__)<<x;Dbg::instance().done();}else;

#define vdbg(x)\
	if(Dbg::instance().isSet(Dbg::Verbose, Dbg::any)){\
	Dbg::instance().print('V', Dbg::any, __FILE__, CRT_FUNCTION_NAME, __LINE__)<<x;Dbg::instance().done();}else;
#define vdbgx(a,x)\
	if(Dbg::instance().isSet(Dbg::Verbose, a)){\
	Dbg::instance().print('V', a,  __FILE__, CRT_FUNCTION_NAME, __LINE__)<<x;Dbg::instance().done();}else;

#ifdef UTRACE
#define tdbgi(a,x)\
	DbgTraceTest __dbgtrace__(a,  __FILE__, CRT_FUNCTION_NAME, __LINE__);\
	if(Dbg::instance().isSet(Dbg::Trace, a)){\
	Dbg::instance().printTraceIn('T', a,  __FILE__, CRT_FUNCTION_NAME, __LINE__)<<x;Dbg::instance().doneTraceIn();}else{\
	__dbgtrace__.v = 0;}

#define tdbgo(a,x)\
	if(Dbg::instance().isSet(Dbg::Trace, a)){\
	__dbgtrace__.v = 0;\
	Dbg::instance().printTraceOut('T', a,  __FILE__, CRT_FUNCTION_NAME, __LINE__)<<x;Dbg::instance().doneTraceOut();}else;

#else
#define tdbgi(a,x)
#define tdbgo(a,x)
#endif

#define pdbg(f, ln, fnc, x)\
	{Dbg::instance().print('P', Dbg::any,  f, fnc, ln)<<x;Dbg::instance().done();}

#define writedbg(x,sz)
#define writedbgx(a, x, sz)

#define check_call(a, v, c) \
	if((c) != v){\
		edbgx(a, "Error call ##c"<<strerror(errno))\
		cassert(false);\
	}

#else

#define idbg(x)
#define idbgx(a,x)
#define edbg(x)
#define edbgx(a,x)
#define wdbg(x)
#define wdbgx(a,x)
#define rdbg(x)
#define rdbgx(a,x)
#define vdbg(x)
#define vdbgx(a,x)
#define tdbgi(a,x)
#define tdbgo(a,x)
#define pdbg(f, ln, fnc, x)

#define writedbg(x,sz)
#define writedbgx(a, x, sz)

#define check_call(a, v, c) c

#endif

#endif

