// frame/objectselector.hpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
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
	
	bool init(ulong _cp);
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
