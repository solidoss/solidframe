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

#include "clientserver/core/common.hpp"
#include "clientserver/core/objptr.hpp"
// #include "system/timespec.hpp"
// #include "utility/queue.hpp"

namespace clientserver{
namespace tcp{

class Connection;
class ChannelData;
class DataNode;

typedef ObjPtr<Connection>	ConnectionPtrTp;


//! A selector for tcp::Connection, that uses epoll on linux.
/*!
	It is designed to work with clientserver::SelectPool
*/
class ConnectionSelector{
public:
	static DataNode* popDataNode();
	static void push(DataNode *_pdn);
	static ChannelData* popChannelData();
	static void push(ChannelData *);
	
	typedef ConnectionPtrTp		ObjectTp;
	ConnectionSelector();
	~ConnectionSelector();
	int reserve(ulong _cp);
	//signal a specific object
	void signal(uint _pos = 0);
	void run();
	uint capacity()const;
	uint size() const;
	int  empty()const;
	int  full()const;
	
	void push(const ConnectionPtrTp &_rcon, uint _thid);
	void prepare();
	void unprepare();
private:
	DataNode* doPopDataNode();
	void doPush(DataNode *_pdn);
	ChannelData* doPopChannelData();
	void doPush(ChannelData *);
private://data
	struct Data;
	Data	&d;
};


}//namespace tcp
}//namespace clientserver

#endif

