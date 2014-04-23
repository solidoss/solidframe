// protocol/text/src/logger.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "protocol/text/logger.hpp"
#include "protocol/text/parameter.hpp"
#include "system/cstring.hpp"
#include "system/cassert.hpp"

using namespace std;

namespace solid{
namespace protocol{
namespace text{

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
			_rs += char_to_cstring(*from);
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
		ins += char_to_cstring(_c);
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
		outs += char_to_cstring(_c1);
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
		outs += char_to_cstring(_c1);
	if(_c2 == ' ')
		outs += _c2;
	else
		outs += char_to_cstring(_c2);
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
		outs += char_to_cstring(_c1);
	if(_c2 == ' ')
		outs += _c2;
	else
		outs += char_to_cstring(_c2);
	if(_c3 == ' ')
		outs += _c3;
	else
		outs += char_to_cstring(_c3);
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
		outs += char_to_cstring(_c1);
	if(_c2 == ' ')
		outs += _c2;
	else
		outs += char_to_cstring(_c2);
	if(_c3 == ' ')
		outs += _c3;
	else
		outs += char_to_cstring(_c3);
	if(_c4 == ' ')
		outs += _c4;
	else
		outs += char_to_cstring(_c4);
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
#ifdef NINLINES
#include "protocol/text/parameter.ipp"
#endif

}//namespace text
}//namespace protocol
}//namespace solid
