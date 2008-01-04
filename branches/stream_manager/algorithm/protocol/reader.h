/* Declarations file reader.h
	
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

#ifndef ALGORITHM_PROTOCOL_READER_H
#define ALGORITHM_PROTOCOL_READER_H

#include "algorithm/protocol/parameter.h"
#include "algorithm/protocol/logger.h"
#include "utility/stack.h"

struct OStreamIterator;

namespace protocol{

class Reader;
//Usefull for fetching lists of strings
struct DummyKey{
	static DummyKey* pointer();
	void initReader(const Reader &);
};

class Reader{
public:
	enum ReturnValues{
		Bad = -1, //!<input closed
		Ok = 0, //!<everything ok, do a pop
		No,		//!<Must wait
		Continue, //!<reexecute the top function - no pop
		Error,		//!<parser error - must enter error recovery
	};
	enum ManagementOptions{
		ClearLogging,
		ResetLogging,
		ClearTmpString,
	};
	enum BasicErrors{
		Unexpected,
		StringTooLong,
		NotANumber,
		LastBasicError,
		IOError
	};
	typedef int (*FncTp)(Reader&, Parameter &_rp);
public:
	Reader(Logger *_plog = NULL, char *_pb = NULL, unsigned _bs = 0);
	virtual ~Reader();
	void push(FncTp _pf, const Parameter & _rp/* = Parameter()*/);
	void replace(FncTp _pf, const Parameter & _rp = Parameter());
	Parameter &push(FncTp _pf);
	unsigned empty()const{return fs.empty();}
	int run();
	bool isRecovering()const;
	bool isError()const;
	bool isLogActive()const{ return plog != NULL;}

	void resetState();
//reading static functions
	static int checkChar(Reader &_rr, Parameter &_rp);
	static int fetchLiteralDummy(Reader &_rr, Parameter &_rp);
	static int fetchLiteralString(Reader &_rr, Parameter &_rp);
	static int fetchLiteralStream(Reader &_rr, Parameter &_rp);
	static int refill(Reader &_rr, Parameter &_rp);
	static int refillDone(Reader &_rr, Parameter &_rp);
	static int pop(Reader &_rr, Parameter &_rp);
	static int returnValue(Reader &_rr, Parameter &_rp);
	static int manage(Reader &_rr, Parameter &_rp);
	static int fetchUint32(Reader &_rr, Parameter &_rp);
	static int dropChar(Reader &_rr, Parameter &_rp);
	static int saveCurrentChar(Reader &_rr, Parameter &_rp);
	//! External reading plugin
	/*! You'd better have one in the most derived reader too*/
	template <class C>
	static int reinit(Reader &_rr, Parameter &_rp){
		return reinterpret_cast<C*>(_rp.a.p)->reinitReader(_rr, _rp);
	}
    
	template <class Rdr, class Creator, class Filter, class Key/* = DummyKey*/>
	static int fetchKey(Reader &_rr, Parameter &_rp){
		Rdr	&rr(static_cast<Rdr&>(_rr));
		int rv = rr.fetch<Filter>(_rr.tmp, _rp.b.u);
		switch(rv){
			case Ok:break;
			case No:
				_rr.push(&Reader::refill);
				return Continue;
			case Error:
				_rr.basicError(StringTooLong);
				return Error;
		}
		Key* pk = reinterpret_cast<Creator*>(_rp.a.p)->create(_rr.tmp);
		if(pk){
			_rr.fs.pop();
			_rr.tmp.clear();
			pk->initReader(rr);
			return Continue;
		}else{
			_rr.keyError(_rr.tmp);
			_rr.tmp.clear();
			return Error;
		}
	}
	template <class Filter>
	static int fetchFilteredString(Reader &_rr, Parameter &_rp){
		int rv = _rr.fetch<Filter>(*static_cast<String*>(_rp.a.p), _rp.b.u);
		switch(rv){
			case Ok:break;
			case No:
				_rr.push(&Reader::refill);
				return Continue;
			case Error:
				_rr.basicError(StringTooLong);
				return Error;
		}
		return Ok;
	}
	
	template <class Filter>
	static int checkIfCharThenPop(Reader &_rr, Parameter &_rp){
		int c;
		if(_rr.peek(c)){
			_rr.push(&Reader::refill);
			return Continue;
		}
		if(Filter::check(c)){
			return pop(_rr, _rp);
		}
		return Ok;
	}
	
protected:
	enum States{
		RunState,
		RecoverState,
		ErrorState,
		FatalErrorState,
		IOErrorState
	};
protected:
	int peek(int &_c);
	int get(int &_c);
	void drop();
	int fetchLiteral(String &_rds, ulong &_rdsz);
	template<class Filter>
	int fetch(String &_rds, ulong _maxsz){
		const char *tbeg = rpos;
		while(rpos != wpos && Filter::check(*(rpos))) ++rpos;
		if((_rds.size() + (rpos - tbeg)) <= _maxsz){
			_rds.append(tbeg, rpos - tbeg);
			if(rpos != wpos){
				if(dolog) plog->readLiteral(_rds.data(),_rds.size());
				return Ok;
			}
		}else{
			if(dolog) plog->readLiteral(_rds.data(), _rds.size());
			return Error;
		}
		return No;
	}
	template <class Filter>
	int locate(String &_rds, ulong &_rdsz, int _keep){
		//int rcode;
		if(_rdsz < (ulong)(bend - bbeg)) return Bad;
		const char *tbeg = rpos;
		while(rpos != wpos && !Filter::check(*rpos)) ++rpos;
		int slen = rpos - tbeg;
		if(slen < _keep){
			if(_rds.size() >= (unsigned)_keep){
				_rds.erase(0, _rds.size() - _keep + slen);
			}
			_rds.append(tbeg, slen);
		}else{
			_rds.assign(rpos - _keep, _keep);
		}
		_rdsz -= slen;
		if(rpos != wpos){
			if(dolog) plog->readLocate(_rds.data(), _rds.size());
			return Ok;
		}
		return No;
	}
	virtual int read(char *_pb, uint32 _bl) = 0;
	virtual int read(OStreamIterator &_rosi, uint64 _sz, char *_pb, uint32 _bl) = 0;
	virtual int readSize()const = 0;
	virtual int doManage(int _mo);
	virtual void prepareErrorRecovery();
	virtual void charError(char _popc, char _expc) = 0;
	virtual void keyError(const String &_pops, int _id = Unexpected) = 0;
	virtual void basicError(int _id) = 0;
protected:
	typedef std::pair<FncTp, Parameter>	FncPairTp;
	Logger				*plog;
	char				*bbeg; //buff begin
	char				*bend;
	char				*rpos;//write buff pos
	char				*wpos;//read buff pos
	short				state;
	bool				dolog;
	Stack<FncPairTp>	fs;
	String				tmp;
};

}// namespace protocol

#endif
