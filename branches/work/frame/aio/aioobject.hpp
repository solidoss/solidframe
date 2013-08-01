// frame/aio/aioobject.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_OBJECT_HPP
#define SOLID_FRAME_AIO_OBJECT_HPP

#include "system/timespec.hpp"
#include "frame/object.hpp"

namespace solid{
namespace frame{
namespace aio{

class Socket;
class SocketPointer;
class Selector;
//! aio::Object is the base class for all classes doing asynchronous socket io
/*!
	Although it can be inherited directly, one should use the extended
	classes aio::SingleObject or aio::MultiObject. It is designed together with
	aio::Selector so the objects shoud stay within a SelectorPool\<aio::Selector>.
	It allows for multiple sockets but this is only from the aio::Selector side,
	as actual support for single/multiple sockets must came from upper levels,
	i.e. inheritants - see aio::SingleObject or aio::MultiObject.
	
*/
class Object: public Dynamic<Object, frame::Object>{
public:
	virtual ~Object();
protected:
	//! Returns true if there are pending io requests
	/*!
		Notable is that the method is thought to be called
		from execute callback, and that its status is reseted
		after exiting from execute.
	*/
	bool hasPendingRequests()const{return reqbeg != reqpos;}
protected:
	friend class Selector;
	//! A stub struncture for sockets
	struct SocketStub{
		enum{
			//Requests from multiconnection to selector
			//Response = 1,
			IORequest = 4,
			UnregisterRequest = 8,
			RegisterRequest = 16,
			AllRequests = 1 + 4 + 8 + 16,
			AllResponses = 2
		};
		SocketStub(Socket *_psock = NULL):
			psock(_psock),
			itimepos(TimeSpec::maximum),
			otimepos(TimeSpec::maximum),
			itoutpos(-1),
			otoutpos(-1),
			hasresponse(false),requesttype(0), chnevents(0), selevents(0){}
		~SocketStub();
		void reset(){
			psock = NULL;
			itimepos = TimeSpec::maximum;
			otimepos = TimeSpec::maximum;
			itoutpos = -1;
			otoutpos = -1;
			hasresponse = false;
			requesttype = 0;
			chnevents = 0;
			selevents = 0;
		}
		Socket		*psock;
		TimeSpec	itimepos;
		TimeSpec	otimepos;
		int			itoutpos;	//-1 or position in toutvec
		int			otoutpos;	//-1 or position in toutvec
		uint8		hasresponse;//the socket is in response queue
		uint8		requesttype;
		int16		state;		//An associated state for the socket
		uint32		chnevents;	//the event from selector to connection:
								//INDONE, OUTDONE, TIMEOUT, ERRDONE
		uint32		selevents;	//used by selector - current io requests
	};
	//!Constructor
	/*!
		Constructor setting all needed data.
		The data is not dealocated, it is the responsability of inheritants.
		E.g. some classes may avoid "new" allocation by holding the table
		within the class (as aio::SingleObject). Others may do a single
		allocation of a big chunck to hold all the tables and just set 
		the pointers to offsets within.
		\param _pstubs A table with allocated stubs, can be changed after
		\param _stubcp The capacity of the table
		\param _reqbeg A table of int32[_stubcp]
		\param _resbeg A table of int32[_stubcp]
		\param _toutbeg A table of int32[_stubcp]
	*/
	Object(
		SocketStub *_pstubs = NULL,
		uint32 _stubcp = 0,
		int32 *_reqbeg = NULL,
		int32 *_resbeg = NULL,
		int32 *_itoutbeg = NULL,
		int32 *_otoutbeg = NULL
	):
		pitimepos(NULL), potimepos(NULL), pstubs(_pstubs), stubcp(_stubcp),
		reqbeg(_reqbeg), reqpos(_reqbeg),
		resbeg(_resbeg), respos(0), ressize(0),
		itoutbeg(_itoutbeg), itoutpos(_itoutbeg),
		otoutbeg(_otoutbeg), otoutpos(_otoutbeg){
	}
	
	void socketPushRequest(const uint _pos, const uint8 _req);
	void socketPostEvents(const uint _pos, const uint32 _evs);
private:
	void doPrepare(TimeSpec *_pitimepos, TimeSpec *_potimepos);
	void doUnprepare();
	void doClearRequests();
	
	uint doOnTimeoutRecv(const TimeSpec &_timepos);
	uint doOnTimeoutSend(const TimeSpec &_timepos);
	
	void doPopTimeoutRecv(uint32 _pos);
	void doPopTimeoutSend(uint32 _pos);
	//!Called by selector on certain events
	virtual int execute(ulong _evs, TimeSpec &_rtout) = 0;
protected:
	void doPushTimeoutRecv(uint32 _pos, const TimeSpec &_crttime, ulong _addsec, ulong _addnsec);
	void doPushTimeoutSend(uint32 _pos, const TimeSpec &_crttime, ulong _addsec, ulong _addnsec);
	
	void setSocketPointer(const SocketPointer &_rsp, Socket *_ps);
	Socket* getSocketPointer(const SocketPointer &_rsp);
	
protected:
	TimeSpec			*pitimepos;
	TimeSpec			*potimepos;
	SocketStub			*pstubs;
	uint32				stubcp;
	int32				*reqbeg;
	int32				*reqpos;
	int32				*resbeg;
	uint32				respos;
	uint32				ressize;
	int32				*itoutbeg;
	int32				*itoutpos;
	int32				*otoutbeg;
	int32				*otoutpos;
};

}//namespace aio
}//namespace frame
}//namespace solid

#endif
