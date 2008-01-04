/* Declarations file writer.h
	
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

#ifndef PROTOCOL_WRITER_H
#define PROTOCOL_WRITER_H

#include "algorithm/protocol/parameter.h"
#include "algorithm/protocol/logger.h"
#include "utility/stack.h"

struct IStreamIterator;

namespace protocol{

class Writer{
public:
	typedef int (*FncTp)(Writer&, Parameter &_rp);
	enum ManagementOptions{
		ClearLogging,
		ResetLogging,
	};
	enum ReturnValues{
		Bad = -1,	//!<input closed
		Ok = 0,		//!<everything ok, do a pop
		No,			//!<Must wait
		Continue,	//!<reexecute the top function - no pop
	};
public:
	Writer(Logger *_plog = NULL);
	virtual ~Writer();
	bool isLogActive()const{return plog != NULL;}
	void push(FncTp _pf, const Parameter & _rp /*= Parameter()*/);
	void replace(FncTp _pf, const Parameter & _rp = Parameter());
	Parameter &push(FncTp _pf);
	unsigned empty()const{return fs.empty();}
	int run();
	
	void putChar(char _c1);
	void putChar(char _c1, char _c2);
	void putChar(char _c1, char _c2, char _c3);
	void putChar(char _c1, char _c2, char _c3, char _c4);
    void putSilentChar(char _c1);
	void putSilentChar(char _c1, char _c2);
	void putSilentChar(char _c1, char _c2, char _c3);
	void putSilentChar(char _c1, char _c2, char _c3, char _c4);
	void putString(const char* _s, uint32 _sz);
	void putSilentString(const char* _s, uint32 _sz);
	void put(uint32 _v);
	void putSilent(uint32 _v);
    
//writting static functions
	static int returnValue(Writer &_rw, Parameter &_rp);
	static int flush(Writer &_rw, Parameter &_rp);
	static int flushAll(Writer &_rw, Parameter &_rp);
	static int doneFlush(Writer &_rw, Parameter &_rp);
	static int putStream(Writer &_rw, Parameter &_rp);
	static int putAtom(Writer &_rw, Parameter &_rp);
	static int putChar(Writer &_rw, Parameter &_rp);
	static int manage(Writer &_rw, Parameter &_rp);
	
	template <class C>
    static int reinit(Writer &_rw, Parameter &_rp){
        return reinterpret_cast<C*>(_rp.a.p)->reinitWriter(_rw, _rp);
    }
//convenient stream opperators
	Writer& operator << (char _c);
	Writer& operator << (const char* _s);
	Writer& operator << (const String &_str);
	Writer& operator << (uint32 _v);
	int flushAll();
protected:
	//NOTE: the buffer needs to be flushed before calling putRawString
	static int putRawString(Writer &_rw, Parameter &_rp);
	static int putStreamDone(Writer &_rw, Parameter &_rp);
	int flush();
	void clear();
	virtual int write(char *_pb, uint32 _bl) = 0;
	virtual int write(IStreamIterator&_rit, uint64 _sz, char *_pb, uint32 _bl) = 0;
	virtual int doManage(int _mo);
protected:
	enum States{
		RunState,
		ErrorState
	};
	enum {
		FlushLength = 1024,
		StartLength = 2048,
		MaxDoubleSizeLength = 4096 //th len up to which we can double the size of the buffer
	};
	void resize(uint32 _sz);
	typedef std::pair<FncTp, Parameter>	FncPairTp;
	typedef Stack<FncPairTp> 			FncStackTp;
	Logger				*plog;
	char				*bbeg;
	char				*bend;
	char				*rpos;
	char				*wpos;
	FncStackTp         	fs;
	bool				dolog;
	short				state;
};

}//namespace protocol

#endif
