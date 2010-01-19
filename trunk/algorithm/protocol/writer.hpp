/* Declarations file writer.hpp
	
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

#ifndef PROTOCOL_WRITER_HPP
#define PROTOCOL_WRITER_HPP

#include "algorithm/protocol/parameter.hpp"
#include "algorithm/protocol/logger.hpp"
#include "utility/stack.hpp"
#include <string>

typedef std::string String;
namespace protocol{
//! A nonblocking buffer oriented (not line oriented) protocol response builder
/*!
	Here are some characteristics of the writer:<br>
	- it is designed for nonblocking/asynchrounous usage
	- it is buffer oriented not line oriented
	- it is flexible and easely extensible
	
	<b>Overview:</b><br>
		Internally it uses a stack of pairs of a function pointer and a parameter 
		(protocol::Parameter) whith wich the function will be called.
		
		Every function can in turn push new calls in the stack.
		
		There is a very simple and fast state machine based on the return value
		of scheduled functions. The state machine will exit when, either the buffer must be flushed
		and this cannot be done imediatly (wait to flushed asynchrounously), the stack is empty,
		a function return Writer::Bad.<br>
	
	<b>Usage:</b><br>
		Inherit, implement the virtual methods and extend the writer with new 
		writing functions.<br>
		In your protocol (connection) execute loop:<br>
		> push some writing callbacks<br>
		> (repeatedly) call run until BAD or OK is returned<br>
		<br>
		For writing use the defined operator<<(s) and/or callbacks for sending strings/chars/streams etc.<br>
		- BAD usually means that the connection was/must be closed
		- OK means that the stack is empty - it doesnt mean the data was parsed 
		successfully - an error might have occurred and the parser has successfully recovered 
		(use isError)<br>
		
		
	<b>Notes:</b><br>
		- You can safely use pointers to existing parameters within the stack.
		- For an excelent example see test::alpha::Writer (test/foundation/alpha/src/alpha.(h/cpp)).
		- The << operators must be very carefully used, because althogh the internal buffer will resize accordigly,
		this is not desirable when scalability is important. So it is the problem of the protocol implemetor
		to ensure that the buffer gets flushed before it's filled/resized.
*/
class Writer{
public:
	typedef int (*FncTp)(Writer&, Parameter &_rp);
	enum ManagementOptions{
		ClearLogging,
		ResetLogging,
	};
	enum ReturnValues{
		Bad = BAD,	//!<input closed
		Ok = OK,	//!<everything ok, do a pop
		No = NOK,	//!<Must wait
		Yield,		//!<Must yield the connection
		Continue,	//!<reexecute the top function - no pop
		LastReturnValue
	};
public:
	//!Writer constructor
	Writer(Logger *_plog = NULL);
	//! Writer destructor
	virtual ~Writer();
	//! Check if the log is active.
	bool isLogActive()const{return plog != NULL;}
	//! Sheduller push method
	/*!
		\param _pf A pointer to a function
		\param _rp A const reference to a parameter
	*/
	void push(FncTp _pf, const Parameter & _rp /*= Parameter()*/);
	//! Sheduller replace top method
	/*!
		\param _pf A pointer to a function
		\param _rp A const reference to a parameter
	*/
	void replace(FncTp _pf, const Parameter & _rp = Parameter());
	//! Sheduller push method
	/*!
		\param _pf A pointer to a function
		\retval Parameter Returns a reference to the actual parameter the function will be called with, so you can get pointers to this parameter.
	*/
	Parameter &push(FncTp _pf);
	//! Check if the stack is empty
	bool empty()const{return fs.empty();}
	//! The state machine algorithm
	int run();
	//! Convenient method for puting one char on output
	void putChar(char _c1);
	//! Convenient method for puting two chars on output
	void putChar(char _c1, char _c2);
	//! Convenient method for puting three chars on output
	void putChar(char _c1, char _c2, char _c3);
	//! Convenient method for puting fourth chars on output
	void putChar(char _c1, char _c2, char _c3, char _c4);
    //! Convenient method for silently (no logging) puting one char on output
    void putSilentChar(char _c1);
    //! Convenient method for silently (no logging) puting two chars on output
	void putSilentChar(char _c1, char _c2);
	//! Convenient method for silently (no logging) puting three chars on output
	void putSilentChar(char _c1, char _c2, char _c3);
	//! Convenient method for silently (no logging) puting fourth chars on output
	void putSilentChar(char _c1, char _c2, char _c3, char _c4);
	//! Convenient method for putting a string on the output
	void putString(const char* _s, uint32 _sz);
	//! Convenient method for silently putting a string on the output
	void putSilentString(const char* _s, uint32 _sz);
	//! Convenient method for putting a number on the output
	void put(uint32 _v);
	//! Convenient method for silently putting a number on the output
	void putSilent(uint32 _v);
    
    
    //! Convenient method for putting a number on the output
	void put(uint64 _v);
	//! Convenient method for silently putting a number on the output
	void putSilent(uint64 _v);
//writing callbacks
	//! Callback for returning a certain value
	template <bool B>
	static int returnValue(Writer &_rw, Parameter &_rp);
	//! Callback for trying to flush the buffer
	/*!
		The buffer will be flushed if it is filled above a certain level.
	*/
	static int flush(Writer &_rw, Parameter &_rp);
	//! Callback for forced flush the buffer
	/*!
		Usually this is the last callback called for a response.
	*/
	static int flushAll(Writer &_rw, Parameter &_rp);
	//! Callback called after a flush
	static int doneFlush(Writer &_rw, Parameter &_rp);
	//! Callback for sending a stream
	static int putStream(Writer &_rw, Parameter &_rp);
	//! Callback for sending a string/atom
	static int putAtom(Writer &_rw, Parameter &_rp);
	//! Callback for sending a single char
	static int putChar(Writer &_rw, Parameter &_rp);
	//! Callback for manage opperations
	static int manage(Writer &_rw, Parameter &_rp);
	//! Callback for external (write) opperations
	/*!
		This callback is central for building complex responses and can be used for all kinds
		of opperations from writing, waiting for asynchrounous events/data, computations etc.
		Using this callback one can control the response building from outside writer.
	*/
	template <class C>
    static int reinit(Writer &_rw, Parameter &_rp){
        return reinterpret_cast<C*>(_rp.a.p)->reinitWriter(_rw, _rp);
    }
//convenient stream opperators
	//! Put a char on the output buffer
	Writer& operator << (char _c);
	//! Put a string on the output buffer
	/*!
		Should be use with great care. If you're not sure about the size
		of the string, consider using the putAtom callback.
	*/
	Writer& operator << (const char* _s);
	//! Put a string on the output buffer
	/*!
		Should be use with great care. If you're not sure about the size
		of the string, consider using the putAtom callback.
	*/
	Writer& operator << (const String &_str);
	//! Put a number on the output buffer
	Writer& operator << (uint32 _v);
	//! Put a number on the output buffer
	Writer& operator << (uint64 _v);
	//! Convenient method for flushing the buffer
	int flushAll();
protected:
	//NOTE: the buffer needs to be flushed before calling putRawString
	//! Convenient callback used internaly
	static int putRawString(Writer &_rw, Parameter &_rp);
	//! Convenient callback used internaly
	static int putStreamDone(Writer &_rw, Parameter &_rp);
	int flush();
	void clear();
	//! The writer will call this method when writing data
	virtual int write(char *_pb, uint32 _bl) = 0;
	//! The writer will call this method on manage callback
	virtual int doManage(int _mo);
protected:
	enum States{
		RunState,
		ErrorState
	};
	enum {
		FlushLength = 1024,//!< If buffer data size is above this value, it will be written.
		StartLength = 1024 * 2, //!< The initial buffer capacity.
		MaxDoubleSizeLength = 4096 //!< the length up to which we can double the size of the buffer.
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
