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

#ifndef PROTOCOL_LOGGER_H
#define PROTOCOL_LOGGER_H

#include <string>

namespace protocol{

typedef std::string String;

//! A class for protocol level logging.
/*!
	Not implemented yet - the interface is dummy
*/
class Logger{
public:
	Logger();
	virtual ~Logger();
	void readFlush();
	void readChar(int _c);
	void readAtom(const char *_pb, unsigned _bl);
	void readLiteral(const char *_pb, unsigned _bl);
	void readLocate(const char *_pb, unsigned _bl);
	
	void writeFlush();
	void writeChar(int _c1);
	void writeChar(int _c1, int _c2);
	void writeChar(int _c1, int _c2, int _c3);
	void writeChar(int _c1, int _c2, int _c3, int _c4);
	void writeAtom(const char *_pb, unsigned _bl);
	void writeLiteral(const char *_pb, unsigned _bl);
};

}

#endif

