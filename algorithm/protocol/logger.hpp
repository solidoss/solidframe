/* Declarations file logger.hpp
	
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

#ifndef PROTOCOL_LOGGER_HPP
#define PROTOCOL_LOGGER_HPP

#include "system/common.hpp"
#include <string>

namespace protocol{

//! A class for protocol level logging.
/*!
	Not implemented yet - the interface is dummy
*/
class Logger{
public:
	Logger(uint32 _linesz = 1024);
	virtual ~Logger();
	void inFlush();
	void inChar(int _c);
	void inAtom(const char *_pb, unsigned _bl);
	void inLiteral(const char *_pb, unsigned _bl);
	void inLocate(const char *_pb, unsigned _bl);
	
	void outFlush();
	void outChar(int _c1);
	void outChar(int _c1, int _c2);
	void outChar(int _c1, int _c2, int _c3);
	void outChar(int _c1, int _c2, int _c3, int _c4);
	void outAtom(const char *_pb, unsigned _bl);
	void outLiteral(const char *_pb, unsigned _bl);
protected:
	virtual void doInFlush(const char *_pd, unsigned _dl) = 0;
	virtual void doOutFlush(const char *_pd, unsigned _dl) = 0;
private:
	std::string 	ins;
	std::string		outs;
	const uint32	linesz;
};

}

#endif

