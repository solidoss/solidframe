/* Implementation file aioobject.cpp
	
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

#include "clientserver/aio/aioobject.hpp"
#include "clientserver/aio/src/aiosocket.hpp"
#include "system/cassert.hpp"
namespace clientserver{

namespace aio{

Object::SocketStub::~SocketStub(){
	delete psock;
}

/*virtual*/ Object::~Object(){
}

/*virtual*/ int Object::accept(clientserver::Visitor &_roi){
	return BAD;
}

void Object::pushRequest(uint _pos, uint _req){
	if(pstubs[_pos].request > SocketStub::Response) return;
	pstubs[_pos].request = _req;
	*reqpos = _pos; ++reqpos;
}

inline void Object::doPushResponse(uint32 _pos){
	if(pstubs[_pos].request != SocketStub::Response){
		*respos = _pos; ++respos;
		pstubs[_pos].request = SocketStub::Response;
	}
}
inline void Object::doPopTimeout(uint32 _pos){
	cassert(toutpos != toutbeg);
	--toutpos;
	toutbeg[pstubs[_pos].toutpos] = *toutpos;
#ifdef DEBUG
	*toutpos = -1;
#endif
	pstubs[_pos].toutpos = -1;
}

void Object::doAddSignaledSocketFirst(uint _pos, uint _evs){
	cassert(_pos < stubcp || pstubs[_pos].psock);
	pstubs[_pos].chnevents = _evs;
	if(pstubs[_pos].toutpos >= 0){
		doPopTimeout(_pos);
	}
	doPushResponse(_pos);
}
void Object::doAddSignaledSocketNext(uint _pos, uint _evs){
	cassert(_pos < stubcp || pstubs[_pos].psock);
	pstubs[_pos].chnevents |= _evs;
	if(pstubs[_pos].toutpos >= 0){
		doPopTimeout(_pos);
	}
	doPushResponse(_pos);
}
void Object::doAddTimeoutSockets(const TimeSpec &_timepos){
	//add all timeouted stubs to responses
	const int32 *pend(toutpos);
	for(int32 *pit(toutbeg); pit != pend;){
		cassert(pstubs[*pit].psock);
		if(pstubs[*pit].timepos <= _timepos){
			pstubs[*pit].chnevents |= TIMEOUT;
			doPushResponse(*pit);
			--pend;
			*pit = *pend;
		}else ++pit;
	}
}

void Object::doPrepare(TimeSpec *_ptimepos){
	ptimepos = _ptimepos;
}
void Object::doUnprepare(){
	ptimepos = NULL;
}

void Object::doClearRequests(){
	reqpos = reqbeg;
}
void Object::doClearResponses(){
	respos = resbeg;
}

}//namespace aio;
}//namespace clientserver
