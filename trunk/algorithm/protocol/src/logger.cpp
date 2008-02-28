/* Implementation file logger.cpp
	
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

#include "algorithm/protocol/logger.hpp"
namespace protocol{
//----------------------------------------------------------
Logger::Logger(){
}
Logger::~Logger(){
}
void Logger::readFlush(){
}
void Logger::readChar(int _c){
}
void Logger::readLiteral(const char *_pb, unsigned _bl){
}
void Logger::readLocate(const char *_pb, unsigned _bl){
}
//----------------------------------------------------------
void Logger::writeFlush(){
}
void Logger::writeChar(int _c1){
}
void Logger::writeChar(int _c1, int _c2){
}
void Logger::writeChar(int _c1, int _c2, int _c3){
}
void Logger::writeChar(int _c1, int _c2, int _c3, int _c4){
}
void Logger::writeAtom(const char *_pb, unsigned _bl){
}
void Logger::writeLiteral(const char *_pb, unsigned _bl){
}
//----------------------------------------------------------
}//namespace protocol

#ifndef UINLINES
#include "algorithm/protocol/parameter.hpp"
#include "parameter.ipp"
#endif

