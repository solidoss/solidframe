/* Declarations file listenerselector.hpp
	
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

#ifndef CS_TCP_LISTENERSELECTOR_HPP
#define CS_TCP_LISTENERSELECTOR_HPP

//#include <vector>

#include "clientserver/core/objptr.hpp"

struct pollfd;

namespace clientserver{
namespace tcp{

class Listener;
typedef ObjPtr<Listener>	ListenerPtrTp;

//! A selector for tcp::Listeners, that uses poll on linux/unix.
/*!
	It is designed to work with clientserver::SelectPool
*/
class ListenerSelector{
public:
	typedef ListenerPtrTp		ObjectTp;
	ListenerSelector();
	~ListenerSelector();
	int reserve(ulong _cp);
	//signal a specific object
	void signal(uint _pos = 0);
	void run();
	uint capacity()const;
	uint size() const;
	int  empty()const;
	int  full()const;
	void prepare();
	void unprepare();
	void push(const ListenerPtrTp &_rlis, uint _thid);
private:
	int doReadPipe();
	int doFullScan();
	int doSimpleScan(int);
private:
	struct Data;
	Data &d;
};

}//namespace tcp
}//namespace clientserver

#endif
