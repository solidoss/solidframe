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
#include "clientserver/core/object.hpp"

namespace clientserver{
namespace aio{

class Socket;
class Selector;

class Object: public clientserver::Object{
public:
	virtual ~Object();
	virtual int execute(ulong _evs, TimeSpec &_rtout) = 0;
	virtual int accept(clientserver::Visitor &_roi);
	bool hasPendingRequests()const{return reqbeg != reqpos;}
protected:
	friend class Selector;
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
		int16		state;
		uint32		chnevents;	//the event from selector to connection:
								//INDONE, OUTDONE, TIMEOUT, ERRDONE
		uint32		selevents;	//used by selector - current io requests
	};
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
}//namespace clientserver

#endif
