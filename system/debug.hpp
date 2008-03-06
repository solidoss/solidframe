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

#ifndef DEBUGPP_HPP
#define DEBUGPP_HPP

void initDebug(const char * _fname);

#ifdef	UDEBUG

#include <stdio.h>
#include <iostream>
#include "timespec.hpp"

struct Dbg{
	static const int fileoff;

	static void lock(TimeSpec &);
	static void unlock();
	static int crtThrId();
};

#define CERR	std::clog

#define pdbg(x) {Dbg::lock();CERR<<x<<std::endl;Dbg::unlock();}
//#define wdbg(x,sz) {Dbg::lock();CERR.write(x,sz);Dbg::unlock();}

#ifndef UTHREADS

inline void Dbg::lock(){}
inline void Dbg::unlock(){}
inline int  Dbg::crtThrId(){return -1;}

#define idbg(x) {Dbg::lock(); CERR<<"I["<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<' '<<x<<std::endl; Dbg::unlock();}


#define edbg(x) {Dbg::lock(); CERR<<"E["<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<' '<<x<<std::endl; Dbg::unlock();}

#else

#define idbg(x) {TimeSpec t;Dbg::lock(t); CERR<<"I["<<t.seconds()<<"."<<t.nanoSeconds()/1000<<" "<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<'('<<Dbg::crtThrId()<<')'<<' '<<x<<std::endl; Dbg::unlock();}
#define edbg(x) {TimeSpec t;Dbg::lock(t); CERR<<"E["<<t.seconds()<<"."<<t.nanoSeconds()/1000<<" "<<(__FILE__ + Dbg::fileoff)<<':'<<__LINE__<<'|'<<__FUNCTION__<<']'<<'('<<Dbg::crtThrId()<<')'<<' '<<x<<std::endl; Dbg::unlock();}
#define wdbg(x,sz) {TimeSpec t;Dbg::lock(t);CERR.write(x,sz);Dbg::unlock();}


#endif

#else

#define pdbg(x) {}
#define idbg(x) {}
#define edbg(x)	{}
#define wdbg(x,sz) {}
#endif

#endif

