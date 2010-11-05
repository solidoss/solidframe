/* Declarations file objectselector.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FOUNDATION_OBJECT_SELECTOR_HPP
#define FOUNDATION_OBJECT_SELECTOR_HPP

#include <vector>
#include <stack>

#include "system/timespec.hpp"
#include "system/condition.hpp"
#include "system/mutex.hpp"

#include "utility/queue.hpp"

#include "foundation/objectpointer.hpp"

#include "foundation/common.hpp"

namespace foundation{

typedef ObjectPointer<Object> ObjectPtrT;

//! An object selector to be used with the template SelectPool
/*!
	A selector will help SelectPool to actively hold objects.
	A selector must export a certain interface requested by the SelectPool,
	and the pool will have one for its every thread.
*/
class ObjectSelector{
public:
	
	typedef ObjectPtrT		ObjectT;
	
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
	
	void push(const ObjectPtrT &_rlis, uint _thid);
private:
	int doWait(int _wt);
	int doExecute(unsigned _i, ulong _evs, TimeSpec _crttout);
private:
	enum {EXIT_LOOP = 1, FULL_SCAN = 2, READ_PIPE = 4};
	struct SelObject{
		ObjectPtrT	objptr;
		TimeSpec	timepos;
		int			state;
	};
	typedef std::vector<SelObject>					SelVecT;
	typedef std::stack<uint, std::vector<uint> >	FreeStackT;
	typedef Queue<uint>								ObjQueueT;
	uint 		sz;
	SelVecT	sv;
	Queue<uint>	uiq;
	FreeStackT	fstk;
	Mutex		mtx;
	Condition	cnd;
	uint64		btimepos;//begin time pos
    TimeSpec	ntimepos;//next timepos == next timeout
    TimeSpec	ctimepos;//current time pos
    ObjQueueT	objq;
};

}

#endif
