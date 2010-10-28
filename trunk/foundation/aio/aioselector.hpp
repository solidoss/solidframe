/* Declarations file selector.hpp
	
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

#ifndef FOUNDATION_AIO_SELECTOR_HPP
#define FOUNDATION_AIO_SELECTOR_HPP

#include "system/timespec.hpp"
#include "foundation/common.hpp"
#include "foundation/objectpointer.hpp"


struct TimeSpec;

namespace foundation{
namespace aio{

class Object;
class Socket;

typedef ObjectPointer<Object>	ObjectPtrT;

//! An asynchronous IO selector to be used with the template SelectPool
/*!
	A selector will help SelectPool to actively hold aio::objects.
	A selector must export a certain interface requested by the SelectPool,
	and the pool will have one for its every thread.
*/
class Selector{
public:
	typedef ObjectPtrT		ObjectT;
	
	Selector();
	~Selector();
	int reserve(ulong _cp);
	//signal a specific object
	void signal(uint _pos = 0)const;
	void run();
	uint capacity()const;
	uint size() const;
	bool empty()const;
	bool full()const;
	
	void push(const ObjectT &_rcon, uint _thid);
	void prepare();
	void unprepare();
private:
	uint doReadPipe();
	uint doAllIo();
	uint doFullScan();
	uint doExecuteQueue();
	uint doNewStub();
	
	void doUnregisterObject(Object &_robj, int _lastfailpos = -1);
	uint doIo(Socket &_rsock, ulong _evs);
	uint doExecute(const uint _pos);
	void doPrepareObjectWait(const uint _pos, const TimeSpec &_timepos);
private://data
	struct Stub;
	struct Data;
	Data	&d;
};


}//namespace aio
}//namespace foundation

#endif

