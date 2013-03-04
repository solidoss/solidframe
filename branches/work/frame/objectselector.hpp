/* Declarations file objectselector.hpp
	
	Copyright 2007, 2008, 2013 Valentin Palade 
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

#ifndef SOLID_FRAME_OBJECT_SELECTOR_HPP
#define SOLID_FRAME_OBJECT_SELECTOR_HPP

#include "utility/dynamicpointer.hpp"
#include "frame/common.hpp"
#include "frame/selectorbase.hpp"


namespace solid{
namespace frame{

typedef DynamicPointer<Object> ObjectPointerT;

//! An object selector to be used with the template SelectPool
/*!
	A selector will help SelectPool to actively hold objects.
	A selector must export a certain interface requested by the SelectPool,
	and the pool will have one for its every thread.
*/
class ObjectSelector: public SelectorBase{
public:
	
	typedef ObjectPointerT	JobT;
	typedef Object			ObjectT;
	
	ObjectSelector();
	
	~ObjectSelector();
	
	int init(ulong _cp);
	//signal a specific object
	void raise(uint32 _pos);
	void run();
	ulong capacity()const;
	ulong size() const;
	bool  empty()const;
	bool  full()const;
	void prepare();
	void unprepare(){}
	
	bool push(ObjectPointerT &_rjob);
private:
	int doWait(int _wt);
	int doExecute(unsigned _i, ulong _evs, TimeSpec _crttout);
private:
	struct Data;
	Data	&d;
};

}//namespace frame
}//namespace solid

#endif
