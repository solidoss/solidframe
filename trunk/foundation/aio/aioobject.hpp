/* Declarations file aioobject.hpp
	
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

#ifndef CS_AIO_OBJECT_HPP
#define CS_AIO_OBJECT_HPP

#include "system/timespec.hpp"
#include "foundation/object.hpp"

namespace foundation{
namespace aio{

class Socket;
class Selector;
//! aio::Object is the base class for all classes doing asynchronous socket io
/*!
	Although it can be inherited directly, one should use the extended
	classes from aio::tcp and aio::udp. It is designed together with
	aio::Selector so the objects shoud stay within a SelectorPool\<aio::Selector>.
	It allows for multiple sockets but this is only from the aio::Selector side,
	as actual support for single/multiple sockets must came from upper levels,
	i.e. inheritants - see aio::tcp::Connection, aio::tcp::MultiConnection and/or
	aio::udp::Talker, aio::udp::MultiTalker.
	
*/
class Object: public foundation::Object{
public:
	virtual ~Object();
	//!Called by selector on certain events
	virtual int execute(ulong _evs, TimeSpec &_rtout) = 0;
	virtual int accept(foundation::Visitor &_roi);
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
			Response = 1,
			IORequest = 4,
			UnregisterRequest = 8,
			RegisterRequest = 16,
			AllRequests = 1 + 4 + 8 + 16,
			AllResponses = 2
		};
		SocketStub(Socket *_psock = NULL):
			psock(_psock),
			timepos(0xffffffff, 0xffffffff),
			toutpos(-1),
			request(0), chnevents(0), selevents(0){}
		~SocketStub();
		void reset(){
			psock = NULL;
			timepos.set(0xffffffff, 0xffffffff);
			toutpos = -1;
			request = 0;
			chnevents = 0;
			selevents = 0;
		}
		Socket		*psock;
		TimeSpec	timepos;
		int			toutpos;	//-1 or position in toutvec
		uint16		request;	//Requests from connection to selector
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
		within the class (as aio::tcp::Connection). Others may do a single
		allocation of a big chunck to hold all the tables and just set 
		the pointers to offsets within.
		\param _pstubs A table with allocated stubs, can be changed after
		\param _stubcp The capacity of the table
		\param _reqbeg A table of int32[_stubcp]
		\param _resbeg A table of int32[_stubcp]
		\param _toutbeg A table of int32[_stubcp]
	*/
	Object(
		SocketStub *_pstubs,
		uint32 _stubcp,
		int32 *_reqbeg,
		int32 *_resbeg,
		int32 *_toutbeg
	):
		ptimepos(NULL), pstubs(_pstubs), stubcp(_stubcp),
		reqbeg(_reqbeg), reqpos(_reqbeg),
		resbeg(_resbeg), respos(_resbeg),
		toutbeg(_toutbeg), toutpos(_toutbeg){}
		void pushRequest(uint _pos, uint _req);
private:
	void doAddSignaledSocketFirst(uint _pos, uint _evs);
	void doAddSignaledSocketNext(uint _pos, uint _evs);
	void doAddTimeoutSockets(const TimeSpec &_timepos);
	void doPrepare(TimeSpec *_ptimepos);
	void doUnprepare();
	void doPushResponse(uint32 _pos);
	void doPopTimeout(uint32 _pos);
	void doClearRequests();
	void doClearResponses();
protected:
	TimeSpec			*ptimepos;
	SocketStub			*pstubs;
	uint32				stubcp;
	int32				*reqbeg;
	int32				*reqpos;
	int32				*resbeg;
	int32				*respos;
	int32				*toutbeg;
	int32				*toutpos;
};

}//namespace aio
}//namespace foundation

#endif
