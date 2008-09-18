/* Declarations file debug.hpp
	
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

#ifndef SYSTEM_DEBUG_HPP
#define SYSTEM_DEBUG_HPP

void initDebug(const char * _fname, const char *_opt = 0);
void printDebugBits();

#ifdef	UDEBUG
void setAllDebugBits();
void resetAllDebugBits();
void setDebugBit(unsigned _v);
void resetDebugBit(unsigned _v);

unsigned registerDebugModule(const char *_name);

#define DEBUG_BITSET_SIZE 256

#include <cstdio>
#include <iostream>
#include "timespec.hpp"

struct Dbg{
	static const unsigned any;
	static const unsigned system;
	static const unsigned specific;
	static const unsigned protocol;
	static const unsigned ser_bin;
	static const unsigned utility;
	static const unsigned cs;
	static const unsigned ipc;
	static const unsigned tcp;
	static const unsigned udp;
	static const unsigned filemanager;
	
	static const int fileoff;

	static bool lock(TimeSpec &);
	static const char* lock(unsigned v, TimeSpec &);
	static const char* lock(unsigned v = 0);
	static void unlock();
	static long crtThrId();
};

#define CERR	std::clog

#ifndef UTHREADS

//inline void Dbg::lock(int v)
inline void Dbg::unlock(){}
inline int  Dbg::crtThrId(){return -1;}
//print
#define pdbg(x) if(Dbg::lock()){CERR<<x<<std::endl;Dbg::unlock();}
#define pdbgx(a,x) if(Dbg::lock(a)){CERR<<x<<std::endl;Dbg::unlock();}

//info
#define idbg(x) if(Dbg::lock()){CERR<<"I["<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<' '<<x<<std::endl; Dbg::unlock();}
#define idbgx(a, x){\
	const char *mod(Dbg::lock(a));\
	if(mod){\
	CERR<<"I["<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<' '<<x<<std::endl; Dbg::unlock();\
	}}
//error
#define edbg(x) if(Dbg::lock()){CERR<<"E["<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<' '<<x<<std::endl; Dbg::unlock();}
#define edbgx(a, x)\
	const char *mod(Dbg::lock(a));\
	if(mod){\
	CERR<<"E-"<<mod<<'['<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<' '<<x<<std::endl; Dbg::unlock();\
	}}
//warn
#define wdbg(x) if(Dbg::lock()){CERR<<"W["<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<' '<<x<<std::endl; Dbg::unlock();}
#define wdbgx(a, x){\
	const char *mod(Dbg::lock(a));\
	if(mod){\
	CERR<<"W-"<<mod<<'['<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<' '<<x<<std::endl; Dbg::unlock();\
	}}
//write
#define writedbg(x,sz) if(Dbg::lock()){CERR.write(x,sz);Dbg::unlock();}
#define writedbgx(a, x, sz) if(Dbg::lock(a)){CERR.write(x,sz);Dbg::unlock();}

#else

//print
#define pdbg(x) {TimeSpec t;if(Dbg::lock(t)){CERR<<x<<std::endl;Dbg::unlock();}}
#define pdbgx(a, x) {TimeSpec t;if(Dbg::lock(a, t)){CERR<<x<<std::endl;Dbg::unlock();}}

//info
#define idbg(x) {\
	TimeSpec t;\
	if(Dbg::lock(t)){ \
	CERR<<"I["<<t.seconds()<<'.'<<t.nanoSeconds()/1000<<" ANY "<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<'('<<Dbg::crtThrId()<<')'<<' '<<x<<std::endl;\
	Dbg::unlock();\
	}}
#define idbgx(a, x) {\
	TimeSpec t;\
	const char *mod(Dbg::lock(a,t));\
	if(mod){ \
	CERR<<'I'<<'['<<t.seconds()<<'.'<<t.nanoSeconds()/1000<<' '<<mod<<' '<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<'('<<Dbg::crtThrId()<<')'<<' '<<x<<std::endl;\
	Dbg::unlock();\
	}}

//error
#define edbg(x) {\
	TimeSpec t;\
	if(Dbg::lock(t)){\
	CERR<<"E["<<t.seconds()<<'.'<<t.nanoSeconds()/1000<<" ANY "<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<'('<<Dbg::crtThrId()<<')'<<' '<<x<<std::endl;\
	Dbg::unlock();\
	}}
#define edbgx(a, x) {\
	TimeSpec t;\
	const char *mod(Dbg::lock(a,t));\
	if(mod){\
	CERR<<'E'<<'['<<t.seconds()<<'.'<<t.nanoSeconds()/1000<<' '<<mod<<' '<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<'('<<Dbg::crtThrId()<<')'<<' '<<x<<std::endl;\
	Dbg::unlock();\
	}}

//warn
#define wdbg(x) {\
	TimeSpec t;\
	if(Dbg::lock(t)){\
	CERR<<'W'<<'['<<t.seconds()<<'.'<<t.nanoSeconds()/1000<<" ANY "<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<'('<<Dbg::crtThrId()<<')'<<' '<<x<<std::endl;\
	Dbg::unlock();\
	}}
#define wdbgx(a, x) {\
	TimeSpec t;\
	const char *mod(Dbg::lock(a,t));\
	if(mod){\
	CERR<<'W'<<'['<<t.seconds()<<'.'<<t.nanoSeconds()/1000<<' '<<mod<<' '<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<'('<<Dbg::crtThrId()<<')'<<' '<<x<<std::endl;\
	Dbg::unlock();\
	}}

//write
#define writedbg(x, sz) {TimeSpec t;if(Dbg::lock(t)){CERR.write(x,sz);Dbg::unlock();}}
#define writedbgx(a, x,sz) {TimeSpec t;if(Dbg::lock(a, t)){CERR.write(x,sz);Dbg::unlock();}}


#endif

#else

#define pdbg(x)
#define pdbgx(a,x)
#define idbg(x)
#define idbgx(a,x)
#define edbg(x)
#define edbgx(a,x)
#define wdbg(x)
#define wdbgx(a,x)
#define writedbg(x,sz)
#define writedbgx(a, x, sz)
#endif

#endif

