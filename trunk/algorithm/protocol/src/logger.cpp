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
#include "utility/string.hpp"
#include "system/cassert.hpp"

using namespace std;

namespace protocol{

void appendLiteral(string &_rs, const char *_pb, unsigned _bl){
	const char *from = _pb;
	const char *to = _pb + _bl;
	while(from != to){
		if(*from != ' ' && !isprint(*from)){
			unsigned sz = from - _pb;
			if(sz){
				_rs.append(_bl, sz);
				_pb = from;
				_bl -= sz;
			}
			_rs += charToString(*from);
			--sz;
			++_pb;
		}
		++from;
	}
	unsigned sz = from - _pb;
	if(sz){
		_rs.append(_pb, sz);
	}
}
//----------------------------------------------------------
Logger::Logger(uint32 _linesz):linesz(_linesz){
}
Logger::~Logger(){
}
void Logger::inFlush(){
	//cassert(ins.size());
	doInFlush(ins.data(), ins.size());
	ins.clear();
}
void Logger::inChar(int _c){
	if(ins.size() + 12 > linesz){
		ins += '.';ins += '.';ins += '.';
		inFlush();
		ins.clear();
		ins += '.';ins += '.';ins += '.';
	}
	if(_c == ' ')
		ins += _c;
	else
		ins += charToString(_c);
}
void Logger::inAtom(const char *_pb, unsigned _bl){
	while(_bl){
		unsigned towrite = linesz - ins.size() - 12;
		if(towrite >= _bl){
			ins.append(_pb, _bl);
			break;
		}else{
			ins.append(_pb, towrite);
			ins += '.';ins += '.';ins += '.';
			inFlush();
			ins.clear();
			ins += '.';ins += '.';ins += '.';
			_pb += towrite;
			_bl -= towrite;
		}
	}
}
void Logger::inLiteral(const char *_pb, unsigned _bl){
	while(_bl){
		unsigned towrite = linesz - ins.size() - 12;
		if(towrite >= _bl){
			appendLiteral(ins, _pb, _bl);
			break;
		}else{
			appendLiteral(ins, _pb, towrite);
			ins += '.';ins += '.';ins += '.';
			inFlush();
			ins.clear();
			ins += '.';ins += '.';ins += '.';
			_pb += towrite;
			_bl -= towrite;
		}
	}
}
void Logger::inLocate(const char *_pb, unsigned _bl){
	inLiteral(_pb, _bl);
	ins += '.';ins += '.';ins += '.';ins += '.';
}
//----------------------------------------------------------
void Logger::outFlush(){
	//cassert(outs.size());
	doOutFlush(outs.data(), outs.size());
	outs.clear();
}
void Logger::outChar(int _c1){
	if(outs.size() + 12 > linesz){
		outs += '.';outs += '.';outs += '.';
		outFlush();
		outs.clear();
		outs += '.';outs += '.';outs += '.';
	}
	if(_c1 == ' ')
		outs += _c1;
	else
		outs += charToString(_c1);
}
void Logger::outChar(int _c1, int _c2){
	if(outs.size() + 12 > linesz){
		outs += '.';outs += '.';outs += '.';
		outFlush();
		outs.clear();
		outs += '.';outs += '.';outs += '.';
	}
	if(_c1 == ' ')
		outs += _c1;
	else
		outs += charToString(_c1);
	if(_c2 == ' ')
		outs += _c2;
	else
		outs += charToString(_c2);
}
void Logger::outChar(int _c1, int _c2, int _c3){
	if(outs.size() + 12 > linesz){
		outs += '.';outs += '.';outs += '.';
		outFlush();
		outs.clear();
		outs += '.';outs += '.';outs += '.';
	}
	if(_c1 == ' ')
		outs += _c1;
	else
		outs += charToString(_c1);
	if(_c2 == ' ')
		outs += _c2;
	else
		outs += charToString(_c2);
	if(_c3 == ' ')
		outs += _c3;
	else
		outs += charToString(_c3);
}
void Logger::outChar(int _c1, int _c2, int _c3, int _c4){
	if(outs.size() + 12 > linesz){
		outs += '.';outs += '.';outs += '.';
		outFlush();
		outs.clear();
		outs += '.';outs += '.';outs += '.';
	}
	if(_c1 == ' ')
		outs += _c1;
	else
		outs += charToString(_c1);
	if(_c2 == ' ')
		outs += _c2;
	else
		outs += charToString(_c2);
	if(_c3 == ' ')
		outs += _c3;
	else
		outs += charToString(_c3);
	if(_c4 == ' ')
		outs += _c4;
	else
		outs += charToString(_c4);
}
void Logger::outAtom(const char *_pb, unsigned _bl){
	while(_bl){
		unsigned towrite = linesz - ins.size() - 12;
		if(towrite >= _bl){
			outs.append(_pb, _bl);
			break;
		}else{
			outs.append(_pb, towrite);
			outs += '.';outs += '.';outs += '.';
			outFlush();
			outs.clear();
			outs += '.';outs += '.';outs += '.';
			_pb += towrite;
			_bl -= towrite;
		}
	}
}
void Logger::outLiteral(const char *_pb, unsigned _bl){
	while(_bl){
		unsigned towrite = linesz - ins.size() - 12;
		if(towrite >= _bl){
			appendLiteral(outs, _pb, _bl);
			break;
		}else{
			appendLiteral(outs, _pb, towrite);
			outs += '.';outs += '.';outs += '.';
			outFlush();
			outs.clear();
			outs += '.';outs += '.';outs += '.';
			_pb += towrite;
			_bl -= towrite;
		}
	}
}
//----------------------------------------------------------
}//namespace protocol

#ifdef NINLINES
#include "algorithm/protocol/parameter.hpp"
#include "algorithm/protocol/parameter.ipp"
#endif

