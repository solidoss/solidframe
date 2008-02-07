/* Declarations file talkerselector.h
	
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

#ifndef CS_UDP_TALKERSELECTOR_H
#define CS_UDP_TALKERSELECTOR_H

#include <stack>
#include <vector>

#include "clientserver/core/common.h"
#include "clientserver/core/objptr.h"
#include "system/timespec.h"
#include "utility/queue.h"

struct epoll_event;
struct TimeSpec;

namespace clientserver{
namespace udp{

class Talker;
class Station;

typedef ObjPtr<Talker>	TalkerPtrTp;

//! A selector for tcp::Talker, that uses epoll on linux.
/*!
	It is designed to work with clientserver::SelectPool
*/
class TalkerSelector{
public:
	typedef TalkerPtrTp		ObjectTp;
	TalkerSelector();
	~TalkerSelector();
	int reserve(ulong _cp);
	//signal a specific object
	void signal(uint _pos = 0);
	void run();
	uint capacity()const	{return cp - 1;}
	uint size() const		{return sz;}
	int  empty()const		{return sz == 1;}
	int  full()const		{return sz == cp;}
	
	void prepare(){}
	void unprepare(){}
	
	void push(const TalkerPtrTp &_rcon, uint _thid);
private:
	enum {EXIT_LOOP = 1, FULL_SCAN = 2, READ_PIPE = 4};
	struct SelTalker{
		SelTalker():timepos(0),evmsk(0),state(0){}
		void reset();
		TalkerPtrTp		tkrptr;//4
		TimeSpec		timepos;//4
		ulong			evmsk;//4
		//state: 1 means the object is in queue
		//or if 0 in chq check loop means the channel was already checked
		int				state;//4
	};
	
	int doReadPipe();
	int doExecute(SelTalker &_rch, ulong _evs, TimeSpec &_rcrttout, epoll_event &_rev);
	int doIo(Station &_rch, ulong _evs);
private://data
	typedef std::stack<SelTalker*, std::vector<SelTalker*> > 	FreeStackTp;
	typedef Queue<SelTalker*>									ChQueueTp;
	uint			cp;
	uint			sz;
	int				selcnt;
	int				epfd;
	epoll_event 	*pevs;
	SelTalker		*pchs;
	ChQueueTp		chq;
	FreeStackTp		fstk;
	int				pipefds[2];
	//uint64          btimepos;//begin time pos
	TimeSpec        ntimepos;//next timepos == next timeout
	TimeSpec        ctimepos;//current time pos
};


}//namespace tcp
}//namespace clientserver

#endif

