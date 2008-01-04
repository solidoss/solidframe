/* Declarations file debug.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DEBUGPP_H
#define DEBUGPP_H

void initDebug(const char * _fname);

#ifdef	UDEBUG

#include <stdio.h>
#include <iostream>
#include "timespec.h"

struct Gdb{
	static const int fileoff;

	static void lock(TimeSpec &);
	static void unlock();
	static int crtThrId();
};

#define CERR	std::clog

#define pdbg(x) {Gdb::lock();CERR<<x<<std::endl;Gdb::unlock();}
#define wdbg(x,sz) {Gdb::lock();CERR.write(x,sz);Gdb::unlock();}

#ifndef UTHREADS

inline void Gdb::lock(){}
inline void Gdb::unlock(){}
inline int  Gdb::crtThrId(){return -1;}

#define idbg(x) {Gdb::lock(); CERR<<"I["<<(__FILE__ + Gdb::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<' '<<x<<std::endl; Gdb::unlock();}


#define edbg(x) {Gdb::lock(); CERR<<"E["<<(__FILE__ + Gdb::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<' '<<x<<std::endl; Gdb::unlock();}

#else

#define idbg(x) {TimeSpec t;Gdb::lock(t); CERR<<"I["<<t.seconds()<<"."<<t.nanoSeconds()/1000<<" "<<(__FILE__ + Gdb::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<'('<<Gdb::crtThrId()<<')'<<' '<<x<<std::endl; Gdb::unlock();}
#define edbg(x) {TimeSpec t;Gdb::lock(t); CERR<<"E["<<t.seconds()<<"."<<t.nanoSeconds()/1000<<" "<<(__FILE__ + Gdb::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<'('<<Gdb::crtThrId()<<')'<<' '<<x<<std::endl; Gdb::unlock();}

#endif

#else

#define pdbg(x) {}
#define idbg(x) {}
#define edbg(x)	{}
#define wdbg(x,sz) {}
#endif

#endif

