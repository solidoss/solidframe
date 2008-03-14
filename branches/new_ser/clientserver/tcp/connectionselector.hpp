/* Declarations file connectionselector.hpp
	
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

#ifndef CS_TCP_CONNECTIONSELECTOR_HPP
#define CS_TCP_CONNECTIONSELECTOR_HPP

#include <stack>
#include <vector>
#include "clientserver/core/common.hpp"
#include "clientserver/core/objptr.hpp"
#include "system/timespec.hpp"
#include "utility/queue.hpp"

struct epoll_event;

namespace clientserver{
namespace tcp{

class Connection;
class Channel;
class ChannelData;
class DataNode;
class IStreamNode;

typedef ObjPtr<tcp::Connection>	ConnectionPtrTp;


//! A selector for tcp::Connection, that uses epoll on linux.
/*!
	It is designed to work with clientserver::SelectPool
*/
class ConnectionSelector{
public:
	static DataNode* popDataNode();
	static IStreamNode* popIStreamNode();
	static void push(DataNode *_pdn);
	static void push(IStreamNode *_psn);
	static ChannelData* popChannelData();
	static void push(ChannelData *);
	
	typedef ConnectionPtrTp		ObjectTp;
	ConnectionSelector();
	~ConnectionSelector();
	int reserve(ulong _cp);
	//signal a specific object
	void signal(uint _pos = 0);
	void run();
	uint capacity()const	{return cp - 1;}
	uint size() const		{return sz;}
	int  empty()const		{return sz == 1;}
	int  full()const		{return sz == cp;}
	
	void push(const ConnectionPtrTp &_rcon, uint _thid);
	void prepare();
	void unprepare();
private:
	enum {EXIT_LOOP = 1, FULL_SCAN = 2, READ_PIPE = 4};
	struct SelChannel;
	
	int doReadPipe();
	int doExecute(SelChannel &_rch, ulong _evs, TimeSpec &_crttout, epoll_event &_rev);
	int doIo(Channel &_rch, ulong _evs);
	DataNode* doPopDataNode();
	IStreamNode* doPopIStreamNode();
	void doPush(DataNode *_pdn);
	void doPush(IStreamNode *_psn);
	ChannelData* doPopChannelData();
	void doPush(ChannelData *);
private://data
	typedef std::stack<SelChannel*, std::vector<SelChannel*> > 	FreeStackTp;
	typedef Queue<SelChannel*>									ChQueueTp;
	
	typedef std::stack<DataNode*>				DataNodeStackTp;
	typedef std::stack<ChannelData*>			ChannelDataStackTp;
	typedef std::stack<IStreamNode*>			IStreamNodeStackTp;
	
	uint				cp;
	uint				sz;
	int					selcnt;
	int					epfd;
	epoll_event 		*pevs;
	SelChannel			*pchs;
	ChQueueTp			chq;
	FreeStackTp			fstk;
	int					pipefds[2];
	//TimeSpec			btimepos;//begin time pos
	TimeSpec			ntimepos;//next timepos == next timeout
	TimeSpec			ctimepos;//current time pos
	DataNodeStackTp		dnstk, dnastk;
	ChannelDataStackTp	cdstk, cdastk;
	IStreamNodeStackTp	isnstk, isnastk;
};


}//namespace tcp
}//namespace clientserver

#endif

