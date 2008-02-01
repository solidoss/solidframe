/* Declarations file objectselector.h
	
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

#ifndef CS_OBJECT_SELECTOR_H
#define CS_OBJECT_SELECTOR_H

#include <vector>
#include <stack>

#include "system/timespec.h"
#include "utility/queue.h"

#include "clientserver/core/objptr.h"

#include "common.h"

namespace clientserver{

typedef ObjPtr<Object> ObjectPtrTp;
//! An object selector to be used with the template SelectPool
/*!
	A selector will help SelectPool to actively hold objects.
	A selector must export a certain interface requested by the SelectPool,
	and the pool will have one for its every thread.
*/
class ObjectSelector{
public:
	typedef ObjectPtrTp		ObjectTp;
	ObjectSelector();
	~ObjectSelector();
	int reserve(ulong _cp);
	//signal a specific object
	void signal(uint _pos = 0);
	void run();
	uint capacity()const	{return sv.size();}
	uint size() const		{return sz;}
	int  empty()const		{return !sz;}
	int  full()const		{return sz == sv.size();}
	
	void prepare(){}
	void unprepare(){}
	
	void push(const ObjectPtrTp &_rlis, uint _thid);
private:
	int doWait(int _wt);
	int doExecute(unsigned _i, ulong _evs, TimeSpec _crttout);
private:
	enum {EXIT_LOOP = 1, FULL_SCAN = 2, READ_PIPE = 4};
	struct SelObject{
		ObjectPtrTp	objptr;
		TimeSpec	timepos;
		int			state;
	};
	typedef std::vector<SelObject>					SelVecTp;
	typedef std::stack<uint, std::vector<uint> >	FreeStackTp;
	typedef Queue<uint>								ObjQueueTp;
	uint 		sz;
	SelVecTp	sv;
	Queue<uint>	uiq;
	FreeStackTp	fstk;
	Mutex		mtx;
	Condition	cnd;
	uint64		btimepos;//begin time pos
    TimeSpec	ntimepos;//next timepos == next timeout
    TimeSpec	ctimepos;//current time pos
    ObjQueueTp	objq;
};

}

#endif
