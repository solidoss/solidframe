/* Declarations file listenerselector.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CS_TCP_LISTENERSELECTOR_H
#define CS_TCP_LISTENERSELECTOR_H

#include <vector>

#include "clientserver/core/objptr.h"
struct pollfd;

namespace clientserver{
namespace tcp{

class Listener;
/**
 * A selector for tcp::Listeners, that uses poll on linux/unix and iocp on windows.
 */
typedef ObjPtr<Listener>	ListenerPtrTp;
class ListenerSelector{
public:
	typedef ListenerPtrTp		ObjectTp;
	ListenerSelector();
	~ListenerSelector();
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
	void push(const ListenerPtrTp &_rlis, uint _thid);
private:
	int doReadPipe();
	int doFullScan();
	int doSimpleScan(int);
	enum {EXIT_LOOP = 1, FULL_SCAN = 2, READ_PIPE = 4};
	struct SelStation{
		ListenerPtrTp	lisptr;
	};
	uint			cp;
	uint			sz;
	SelStation		*pss;
	pollfd			*pfds;
	int				pipefds[2];
};


}//namespace tcp
}//namespace clientserver

#endif
